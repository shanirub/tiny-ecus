#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Log a one-time boot banner: node role, app name and version, chip model and
 * revision, and the ESP-IDF version the image was built with.
 *
 * @param role short human-readable role, e.g. "wheel node" or "gateway"
 */
void tiny_common_print_banner(const char *role);

#ifdef __cplusplus
}
#endif
