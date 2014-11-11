#pragma once

typedef void (*SampleFunc)();

void bunny_sample();
void cubes_sample();
void sponza_sample();

static SampleFunc g_samples[] {
    cubes_sample,
    /* bunny_sample, */
    /* sponza_sample, */
};

static size_t g_num_samples = sizeof(g_samples) / sizeof(SampleFunc);
