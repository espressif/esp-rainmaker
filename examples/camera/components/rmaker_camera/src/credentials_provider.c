/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "credentials_provider.h"
#include "esp_log.h"
#include <esp_rmaker_core.h>
#include <esp_rmaker_factory.h>
#include <esp_rmaker_work_queue.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char *TAG = "credentials";

/* Global credentials pointer - allows direct access without copying */
static esp_rmaker_aws_credentials_t *g_current_credentials = NULL;

/* Timeout for work queue operations (milliseconds) */
#define SECURITY_TOKEN_TIMEOUT_MS 10000

/* Context for the get_aws_security_token work-queue operation.
 *
 * Lifetime is refcounted (init 2): the caller and the work fn each hold one
 * reference and drop it via security_token_ctx_release() when done; whoever
 * drops the last reference frees ctx + the semaphore. This closes the timeout
 * race — if the caller times out and returns while the work fn is still
 * queued/running, ctx (and the semaphore the work fn will xSemaphoreGive)
 * stays alive until the work fn also finishes, so the work fn never writes
 * ctx->result / gives a freed semaphore. On success the caller transfers
 * result ownership (sets result = NULL) before releasing. */
typedef struct {
    const char *role_alias;
    esp_rmaker_aws_credentials_t *result;
    SemaphoreHandle_t semaphore;
    atomic_int refcount;
} security_token_ctx_t;

/* Drop one reference; the party dropping the last one frees ctx, its (orphan)
 * result, and the semaphore. atomic_fetch_sub returns the pre-decrement value,
 * so == 1 means we were the last holder. */
static void security_token_ctx_release(security_token_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    if (atomic_fetch_sub(&ctx->refcount, 1) != 1) {
        return;   /* another holder remains */
    }
    if (ctx->result) {
        esp_rmaker_free_aws_credentials(ctx->result);
        ctx->result = NULL;
    }
    if (ctx->semaphore) {
        vSemaphoreDelete(ctx->semaphore);
        ctx->semaphore = NULL;
    }
    free(ctx);
}

/* Work function to perform get_aws_security_token in work queue context */
static void get_security_token_work_fn(void *priv_data)
{
    security_token_ctx_t *ctx = (security_token_ctx_t *)priv_data;
    if (!ctx) {
        return;
    }

    /* Blocking HTTP call; may outlive the caller's wait — ctx stays alive
     * because we hold a reference. */
    ctx->result = esp_rmaker_get_aws_security_token(ctx->role_alias);

    /* Wake the caller if still waiting (harmless if it already timed out). */
    xSemaphoreGive(ctx->semaphore);

    /* Drop our reference; frees ctx iff the caller already dropped its. */
    security_token_ctx_release(ctx);
}

/* Wrapper function to get AWS security token using work queue */
static esp_rmaker_aws_credentials_t *esp_rmaker_get_aws_security_token_safe(const char *role_alias)
{
    if (!role_alias) {
        return NULL;
    }

    /* Create semaphore for synchronization */
    SemaphoreHandle_t semaphore = xSemaphoreCreateBinary();
    if (!semaphore) {
        ESP_LOGE(TAG, "Failed to create semaphore for get_aws_security_token");
        return NULL;
    }

    /* Allocate context on heap so it persists until work function completes */
    security_token_ctx_t *ctx = (security_token_ctx_t *)malloc(sizeof(security_token_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate context for get_aws_security_token");
        vSemaphoreDelete(semaphore);
        return NULL;
    }

    ctx->role_alias = role_alias;
    ctx->result = NULL;
    ctx->semaphore = semaphore;
    atomic_init(&ctx->refcount, 2);   /* one ref for us, one for the work fn */

    /* Try to submit work to queue - if queue is not initialized, fall back to direct call */
    esp_err_t err = esp_rmaker_work_queue_add_task(get_security_token_work_fn, ctx);
    if (err != ESP_OK) {
        /* Work fn will never run (so never takes its ref); caller owns
         * everything. Free directly and fall back to a direct call. */
        ESP_LOGW(TAG, "Work queue not available (err: %d), using direct get_aws_security_token", err);
        vSemaphoreDelete(semaphore);
        free(ctx);
        return esp_rmaker_get_aws_security_token(role_alias);
    }

    esp_rmaker_aws_credentials_t *result = NULL;
    if (xSemaphoreTake(semaphore, pdMS_TO_TICKS(SECURITY_TOKEN_TIMEOUT_MS)) == pdTRUE) {
        /* Work fn completed and handed us the result; take ownership so the
         * final release does not free it. */
        result = ctx->result;
        ctx->result = NULL;
    } else {
        /* Timed out. Do NOT free anything — the work fn is still running and
         * holds its own reference; whichever of us finishes last frees ctx
         * (and any result the work fn eventually produces). */
        ESP_LOGE(TAG, "Timeout waiting for get_aws_security_token to complete");
    }

    security_token_ctx_release(ctx);   /* drop the caller's reference */
    return result;
}

/* Assume the videostream role and get the aws credentials */
int rmaker_fetch_aws_credentials(uint64_t user_data,
                                 const char **pAK, uint32_t *pAKLen,
                                 const char **pSK, uint32_t *pSKLen,
                                 const char **pTok, uint32_t *pTokLen,
                                 uint32_t *pExp)
{
    (void)user_data;

    /* Free previous credentials if any */
    if (g_current_credentials) {
        esp_rmaker_free_aws_credentials(g_current_credentials);
        g_current_credentials = NULL;
    }

    /* Get fresh credentials using work queue to avoid SPIRAM issues */
    g_current_credentials = esp_rmaker_get_aws_security_token_safe("esp-videostream-v1-NodeRole");
    if (!g_current_credentials) {
        ESP_LOGE(TAG, "Failed to get AWS security token");
        return -1;
    }

    /* Return pointers directly to credential fields */
    *pAK = g_current_credentials->access_key;
    *pAKLen = (uint32_t)strlen(g_current_credentials->access_key);
    *pSK = g_current_credentials->secret_key;
    *pSKLen = (uint32_t)strlen(g_current_credentials->secret_key);
    *pTok = g_current_credentials->session_token;
    *pTokLen = (uint32_t)strlen(g_current_credentials->session_token);
    *pExp = g_current_credentials->expiration;

    ESP_LOGI(TAG, "Successfully fetched AWS credentials using RainMaker core function");
    return 0;
}
