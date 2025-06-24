idf_component_register(SRCS "./app_main.cpp"
                            "./op_creds_issuer/app_matter_controller_creds_issuer.cpp"
                            "./rmaker_controller_service/app_matter_controller.cpp"
                            "./rmaker_controller_service/app_matter_controller_callback.cpp"
                            "./rmaker_controller_service/app_matter_device_manager.cpp"
                            "./rmaker_controller_service/matter_controller_std.cpp"
                       INCLUDE_DIRS "." "rmaker_controller_service" "op_creds_issuer")
