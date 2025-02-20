# Exit immediately if any command fails
set -e

# Define the base components directory
COMPONENTS_DIR="./components"

# Create the components directory if it doesn't exist
mkdir -p "$COMPONENTS_DIR"

# Function to clone and checkout a repo
clone_and_checkout() {
    local target_dir="$COMPONENTS_DIR/$1"
    local repo_url="$2"
    local commit_hash="$3"

    # Check if target directory exists, and remove it to force re-clone
    if [ -d "$target_dir" ]; then
        echo "Removing existing $target_dir..."
        rm -rf "$target_dir"
    fi

    # Create target directory
    mkdir -p "$target_dir"

    # Change to target directory
    cd "$target_dir"

    # Clone repository
    echo "Cloning $repo_url into $target_dir..."
    git clone "$repo_url" .

    # Checkout to specified commit
    echo "Checking out $commit_hash..."
    git checkout "$commit_hash"

    # Return to project root
    cd - > /dev/null
}

# Clone and checkout the required repositories
clone_and_checkout "FastLED" "https://github.com/FastLED/FastLED.git" "fb6cd4b2881192fd2c9710f0ae99174c63c0b971"
clone_and_checkout "m5_lvgl_pck/m5_lvgl" "https://github.com/m5stack/lv_m5_emulator.git" "f7cc52e18c275c14773fc902c452187f6841f7b5"
clone_and_checkout "m5_relay_pck/m5_relay" "https://github.com/m5stack/M5Unit-RELAY.git" "63fea6153fa6db13aff1c743403b86f09383fc71"

echo "Repositories successfully cloned and checked out."

