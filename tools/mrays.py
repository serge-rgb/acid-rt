w = 1920
w = 1920
h = 1280
h = 720

def mrays(ms):
    return (w * h * 1000.0/ms) / 1000000

def ms(mrays):
    return (w * h) / (mrays * 1000.0)
