#pragma once


typedef void (*SampleFunc)();

void bunny_sample();
void cubes_sample();

static SampleFunc g_samples[] {
    bunny_sample,
    cubes_sample,
};

static size_t g_num_samples = sizeof(g_samples) / sizeof(SampleFunc);
