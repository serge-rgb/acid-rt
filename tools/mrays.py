w = 1920
h = 1080

def mrays(ms):
    return (1920 * 1080 * 1000.0/ms) / 1000000

def ms(mrays):
    return (1920 * 1080) / (mrays * 1000.0)
