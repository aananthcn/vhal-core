// Stub for Linux port — redirects Android logging to stderr
#pragma once
#include <cstdio>
#ifndef LOG_TAG
#define LOG_TAG "vhal"
#endif
#define ALOGD(fmt, ...) fprintf(stderr, "D/" LOG_TAG ": " fmt "\n", ##__VA_ARGS__)
#define ALOGI(fmt, ...) fprintf(stderr, "I/" LOG_TAG ": " fmt "\n", ##__VA_ARGS__)
#define ALOGW(fmt, ...) fprintf(stderr, "W/" LOG_TAG ": " fmt "\n", ##__VA_ARGS__)
#define ALOGE(fmt, ...) fprintf(stderr, "E/" LOG_TAG ": " fmt "\n", ##__VA_ARGS__)
#define ALOGV(fmt, ...) fprintf(stderr, "V/" LOG_TAG ": " fmt "\n", ##__VA_ARGS__)

#include <cassert>
