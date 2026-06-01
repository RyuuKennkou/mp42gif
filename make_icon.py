"""生成 mp4_to_gif 应用图标 icon.ico"""
from PIL import Image, ImageDraw, ImageFont

def make_icon(size):
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    m = size // 20

    # 圆角矩形背景
    d.rounded_rectangle([m, m, size-m, size-m], radius=size//6, fill='#268c26')

    # 白色播放三角
    cx, cy = size//2, size//2
    r = int(size * 0.22)
    pts = [(cx - r//2, cy - r), (cx + r, cy), (cx - r//2, cy + r)]
    d.polygon(pts, fill='white')

    return img

# 生成多种尺寸并保存为 ico
icons = [make_icon(s) for s in (256, 128, 64, 48, 32, 16)]
icons[0].save('icon.ico', format='ICO', sizes=[(s, s) for s in (256,128,64,48,32,16)],
              append_images=icons[1:])
print('icon.ico created')
