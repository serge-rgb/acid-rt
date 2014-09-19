#pragma once


typedef void (*SampleFunc)();

void cubes_sample();

static SampleFunc g_samples[] {
    cubes_sample,
};

static size_t g_num_samples = sizeof(SampleFunc) / sizeof(SampleFunc);
