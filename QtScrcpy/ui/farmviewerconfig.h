#ifndef FARMVIEWERCONFIG_H
#define FARMVIEWERCONFIG_H

/**
 * FarmViewer Configuration
 *
 * Tunable constants for production deployment with 100+ devices.
 * Adjust these values based on your hardware capabilities and requirements.
 */

namespace FarmViewerConfig {

// ============================================================================
// GRID LAYOUT CONFIGURATION
// ============================================================================

// Maximum columns per device count tier
constexpr int MAX_COLS_TIER_1 = 2;   // 1-4 devices
constexpr int MAX_COLS_TIER_2 = 3;   // 5-9 devices
constexpr int MAX_COLS_TIER_3 = 5;   // 10-20 devices
constexpr int MAX_COLS_TIER_4 = 8;   // 21-50 devices
constexpr int MAX_COLS_TIER_5 = 10;  // 50+ devices

// Tile sizes (width x height) for different device counts
// These maintain a 9:16 aspect ratio (portrait phone)
struct TileSize {
    int width;
    int height;
};

constexpr TileSize TILE_SIZE_1_5_DEVICES   = {300, 600};  // Ultra quality
constexpr TileSize TILE_SIZE_6_20_DEVICES  = {220, 450};  // High quality
constexpr TileSize TILE_SIZE_21_50_DEVICES = {160, 320};  // Medium quality
constexpr TileSize TILE_SIZE_50_PLUS       = {120, 240};  // Low quality (100+ devices)

// Grid spacing and margins
constexpr int GRID_SPACING = 10;       // Space between tiles in pixels
constexpr int GRID_MARGIN = 10;        // Margin around grid in pixels
constexpr int SCROLLBAR_WIDTH = 40;    // Reserved space for scrollbar


// ============================================================================
// STREAM QUALITY CONFIGURATION
// ============================================================================

// Quality Tier 1: Ultra (1-5 devices)
constexpr int QUALITY_ULTRA_RESOLUTION = 1080;
constexpr int QUALITY_ULTRA_BITRATE = 8000000;    // 8 Mbps
constexpr int QUALITY_ULTRA_FPS = 60;

// Quality Tier 2: High (6-20 devices)
constexpr int QUALITY_HIGH_RESOLUTION = 720;
constexpr int QUALITY_HIGH_BITRATE = 4000000;     // 4 Mbps
constexpr int QUALITY_HIGH_FPS = 30;

// Quality Tier 3: Medium (21-50 devices)
constexpr int QUALITY_MEDIUM_RESOLUTION = 540;
constexpr int QUALITY_MEDIUM_BITRATE = 2000000;   // 2 Mbps
constexpr int QUALITY_MEDIUM_FPS = 20;

// Quality Tier 4: Low (50+ devices)
constexpr int QUALITY_LOW_RESOLUTION = 480;
constexpr int QUALITY_LOW_BITRATE = 1000000;      // 1 Mbps
constexpr int QUALITY_LOW_FPS = 15;

// Device count thresholds for quality tier switching
constexpr int THRESHOLD_ULTRA_TO_HIGH = 5;
constexpr int THRESHOLD_HIGH_TO_MEDIUM = 20;
constexpr int THRESHOLD_MEDIUM_TO_LOW = 50;


// ============================================================================
// CONNECTION POOL CONFIGURATION
// ============================================================================

// Maximum number of simultaneous device connections
constexpr int MAX_CONNECTIONS = 200;

// Idle timeout in milliseconds (connections unused for this duration will be cleaned up)
constexpr int IDLE_TIMEOUT_MS = 5 * 60 * 1000;    // 5 minutes

// Cleanup interval in milliseconds (how often to check for idle connections)
constexpr int CLEANUP_INTERVAL_MS = 60 * 1000;    // 1 minute

// Memory warning threshold in bytes
constexpr quint64 MEMORY_WARNING_THRESHOLD = 500 * 1024 * 1024;  // 500 MB

// Estimated memory per connection in bytes (for monitoring)
constexpr quint64 BYTES_PER_CONNECTION = 3 * 1024 * 1024;  // 3 MB


// ============================================================================
// PORT ALLOCATION CONFIGURATION
// ============================================================================

// Starting port for device connections
constexpr int START_PORT = 27183;

// Maximum port (will wrap around to START_PORT)
constexpr int MAX_PORT = 30000;

// Port range size
constexpr int PORT_RANGE = MAX_PORT - START_PORT;


// ============================================================================
// UI CONFIGURATION
// ============================================================================

// Font sizes for different tile sizes
constexpr int FONT_SIZE_LARGE = 11;   // For tiles >= 150px width
constexpr int FONT_SIZE_SMALL = 9;    // For tiles < 150px width

// Minimum window size
constexpr int MIN_WINDOW_WIDTH = 800;
constexpr int MIN_WINDOW_HEIGHT = 600;

// Default window size
constexpr int DEFAULT_WINDOW_WIDTH = 1200;
constexpr int DEFAULT_WINDOW_HEIGHT = 900;

// Label maximum height
constexpr int DEVICE_LABEL_MAX_HEIGHT = 50;


// ============================================================================
// PERFORMANCE TUNING
// ============================================================================

// Maximum devices to show simultaneously (consider pagination beyond this)
constexpr int MAX_VISIBLE_DEVICES = 100;

// Enable/disable features for better performance
constexpr bool ENABLE_RENDER_EXPIRED_FRAMES = false;  // Disable for better performance

// Video buffer size multiplier (1.0 = default, < 1.0 = smaller buffers)
constexpr double VIDEO_BUFFER_MULTIPLIER = 0.8;

// Decoder thread priority (0 = normal, higher = more priority)
constexpr int DECODER_THREAD_PRIORITY = 0;


// ============================================================================
// ADVANCED CONFIGURATION
// ============================================================================

// Connection retry settings
constexpr int MAX_CONNECTION_RETRIES = 3;
constexpr int RETRY_DELAY_MS = 1000;  // 1 second

// Batch connection settings (for connecting many devices at once)
constexpr int BATCH_CONNECT_DELAY_MS = 100;  // Delay between batch connections
constexpr int BATCH_CONNECT_SIZE = 10;       // Number of devices to connect per batch

// Resource monitoring
constexpr bool ENABLE_RESOURCE_MONITORING = true;
constexpr int RESOURCE_CHECK_INTERVAL_MS = 30 * 1000;  // 30 seconds

// Debug settings
constexpr bool ENABLE_DETAILED_LOGGING = false;  // Enable for troubleshooting
constexpr bool LOG_PERFORMANCE_METRICS = false;  // Log FPS, latency, etc.

} // namespace FarmViewerConfig

#endif // FARMVIEWERCONFIG_H
