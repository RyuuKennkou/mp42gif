#!/usr/bin/env python3
"""
MP4 → GIF 转换器
依赖: pip install opencv-python-headless Pillow numpy
打包: pyinstaller --onefile --windowed --name mp4_to_gif mp4_to_gif.py
"""

import os
import threading
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

import cv2
import numpy as np
from PIL import Image, ImageTk


class MP4ToGifConverter:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title('MP4 → GIF 转换器')
        self.root.geometry('900x580')
        self.root.resizable(False, False)
        self.root.configure(bg='white')

        self.font_family = 'Microsoft YaHei'
        self.fsn = 10
        try:
            self.root.option_add('*Font', (self.font_family, self.fsn))
        except Exception:
            self.font_family = 'TkDefaultFont'

        self.input_file = ''
        self.output_file = ''
        self.video_info = {'width': 0, 'height': 0, 'duration': 0.0, 'fps': 0.0}
        self.first_frame_bgr = None
        self.converting = False
        self._updating_ui = False

        self._build_ui()

    # ================================================================
    #  UI 辅助
    # ================================================================

    def _label(self, text, x, y, w, h, bold=False, font_size=None,
               fg=None, anchor='w'):
        fs = font_size if font_size else self.fsn
        ft = (self.font_family, fs, 'bold') if bold else (self.font_family, fs)
        kw = {'text': text, 'bg': 'white', 'font': ft, 'anchor': anchor}
        if fg:
            kw['fg'] = fg
        lbl = tk.Label(self.root, **kw)
        lbl.place(x=x, y=y, width=w, height=h)
        return lbl

    def _edit(self, x, y, w, h, readonly=False, callback=None):
        state = 'readonly' if readonly else 'normal'
        e = tk.Entry(self.root, font=(self.font_family, self.fsn),
                     bg='white', relief='sunken', borderwidth=1,
                     state=state, justify='left')
        e.place(x=x, y=y, width=w, height=h)
        if callback:
            e.bind('<KeyRelease>', callback)
        return e

    def _button(self, text, x, y, w, h, callback,
                font_size=None, bg_color=None, fg_color='white'):
        fs = font_size if font_size else self.fsn
        kw = {'text': text, 'font': (self.font_family, fs),
              'command': callback,
              'relief': 'raised', 'borderwidth': 1}
        if bg_color:
            kw['bg'] = bg_color
            kw['activebackground'] = _darken(bg_color)
            if fg_color:
                kw['fg'] = fg_color
                kw['activeforeground'] = fg_color
        btn = tk.Button(self.root, **kw)
        btn.place(x=x, y=y, width=w, height=h)
        return btn

    def _combo(self, values, x, y, w, h, default_idx=0, callback=None):
        var = tk.StringVar(value=values[default_idx])
        cb = ttk.Combobox(self.root, textvariable=var, values=values,
                          state='readonly', font=(self.font_family, self.fsn))
        cb.place(x=x, y=y, width=w, height=h)
        if callback:
            cb.bind('<<ComboboxSelected>>', callback)
        cb.current(default_idx)
        return cb, var

    def _checkbox(self, text, x, y, w, h, default=True, callback=None):
        var = tk.BooleanVar(value=default)
        cb = tk.Checkbutton(self.root, text=text, variable=var,
                            bg='white', font=(self.font_family, self.fsn),
                            activebackground='white',
                            anchor='w', command=callback)
        cb.place(x=x, y=y, width=w, height=h)
        return cb, var

    # ================================================================
    #  UI 布局
    # ================================================================

    def _build_ui(self):
        ff = self.font_family
        fsn = self.fsn
        edit_w = 480
        btn_x = 505
        btn_w = 110

        # ---- 标题 ----
        self._label('MP4 → GIF 转换器', 15, 8, 620, 30,
                    bold=True, font_size=18, anchor='center')

        # ---- 输入文件 ----
        self._label('输入视频:', 15, 48, 70, 20)
        self.input_edit = self._edit(15, 68, edit_w, 26, readonly=True)
        self._button('浏览...', btn_x, 68, btn_w, 26, self._browse_input)

        # ---- 输出文件 ----
        self._label('输出GIF:', 15, 112, 70, 20)
        self.output_edit = self._edit(15, 132, edit_w, 26,
                                      callback=self._on_output_edited)
        self._button('浏览...', btn_x, 132, btn_w, 26, self._browse_output)

        # ---- 视频信息 ----
        self.video_info_label = self._label(
            '未加载视频', 15, 168, 620, 18, fg='#888888')

        # 分隔线
        self._label('─' * 76, 15, 196, 620, 8, fg='#b0b0b0')

        # ---- 尺寸设置 ----
        self._label('尺寸设置', 15, 216, 80, 20, bold=True, font_size=12)

        self._label('预设:', 15, 250, 40, 18)
        self.preset_combo, self.preset_var = self._combo(
            ['微信表情包 (300px)', '小尺寸 (200px)', '中等 (480px)',
             '大尺寸 (720px)', '原始尺寸', '自定义'],
            55, 247, 185, 25, callback=self._on_preset_changed)

        self.ratio_check, self.ratio_var = self._checkbox(
            '保持原始宽高比 (仅需设置最短边)',
            15, 286, 300, 20, callback=self._on_ratio_changed)

        # ---- 尺寸输入行 (Frame 分组) ----
        row_frame = tk.Frame(self.root, bg='white')
        row_frame.place(x=15, y=318, width=620, height=30)

        self.frame_ratio = tk.Frame(row_frame, bg='white')
        tk.Label(self.frame_ratio, text='最短边:', bg='white',
                 font=(ff, fsn)).pack(side='left', padx=(15, 4))
        self.shortest_edit = tk.Entry(self.frame_ratio, font=(ff, fsn), bg='white',
                                      relief='sunken', borderwidth=1,
                                      justify='center', width=5)
        self.shortest_edit.pack(side='left', padx=(0, 4))
        self.shortest_edit.insert(0, '300')
        self.shortest_edit.bind('<KeyRelease>', self._on_shortest_changed)
        tk.Label(self.frame_ratio, text='像素', bg='white',
                 font=(ff, fsn)).pack(side='left')

        self.frame_manual = tk.Frame(row_frame, bg='white')
        tk.Label(self.frame_manual, text='宽度:', bg='white',
                 font=(ff, fsn)).pack(side='left', padx=(15, 4))
        self.width_edit = tk.Entry(self.frame_manual, font=(ff, fsn), bg='white',
                                   relief='sunken', borderwidth=1,
                                   justify='center', width=5)
        self.width_edit.pack(side='left', padx=(0, 8))
        self.width_edit.bind('<KeyRelease>', self._on_manual_size_changed)
        tk.Label(self.frame_manual, text='高度:', bg='white',
                 font=(ff, fsn)).pack(side='left', padx=(0, 4))
        self.height_edit = tk.Entry(self.frame_manual, font=(ff, fsn), bg='white',
                                    relief='sunken', borderwidth=1,
                                    justify='center', width=5)
        self.height_edit.pack(side='left', padx=(0, 8))
        self.height_edit.bind('<KeyRelease>', self._on_manual_size_changed)
        tk.Label(self.frame_manual, text='像素', bg='white',
                 font=(ff, fsn)).pack(side='left')

        self.frame_ratio.pack(fill='x')

        # ---- 输出尺寸 & 预计大小 ----
        self.size_info_label = self._label(
            '输出尺寸: -- × --    预计大小: --',
            15, 354, 620, 22, bold=True, fg='#1a1aa6')

        # 分隔线
        self._label('─' * 76, 15, 386, 620, 8, fg='#b0b0b0')

        # ---- 画质 & 帧率 ----
        self._label('画质:', 15, 410, 40, 18)
        self.quality_combo, self.quality_var = self._combo(
            ['最佳', '高', '中等', '低'], 55, 407, 100, 25,
            callback=self._on_quality_fps_changed)

        self._label('帧率:', 180, 410, 40, 18)
        self.fps_edit = tk.Entry(self.root, font=(ff, fsn), bg='white',
                                 relief='sunken', borderwidth=1,
                                 justify='center')
        self.fps_edit.place(x=222, y=407, width=50, height=25)
        self.fps_edit.insert(0, '15')
        self.fps_edit.bind('<KeyRelease>', self._on_quality_fps_changed)
        self._label('fps', 276, 410, 25, 18)

        self._label('最佳=256色+抖动 | 高=256色 | 中=128色 | 低=64色',
                    15, 438, 350, 14, fg='#8c8c8c', font_size=8)

        # ---- 转换按钮 ----
        self.convert_btn = self._button(
            '开始转换', 180, 465, 260, 38, self._start_conversion,
            font_size=14, bg_color='#268c26')

        # ---- 进度条 ----
        self.progress_bar = ttk.Progressbar(
            self.root, mode='determinate')
        self.progress_bar.place(x=15, y=515, width=620, height=12)

        # ---- 状态 ----
        self.status_label = self._label(
            '就绪', 15, 536, 620, 22, fg='#666666', anchor='center')

        # ============================================================
        #  右侧: 预览面板
        # ============================================================

        rx = 660
        rw = 225

        self._label('输入视频', rx, 8, rw, 22, bold=True, anchor='center')
        self.input_preview_frame = tk.Frame(
            self.root, bg='#eeeeee',
            highlightbackground='#aaaaaa', highlightthickness=1)
        self.input_preview_frame.place(x=rx+5, y=36, width=rw-10, height=160)
        self.input_preview_label = tk.Label(
            self.input_preview_frame, bg='#eeeeee')
        self.input_preview_label.pack(fill='both', expand=True)

        self._label('↓', rx, 202, rw, 16, fg='#8c8c8c',
                    font_size=12, anchor='center')

        self.output_preview_title = self._label(
            '输出GIF', rx, 224, rw, 22, bold=True, anchor='center')
        self.output_preview_frame = tk.Frame(
            self.root, bg='#eeeeee',
            highlightbackground='#aaaaaa', highlightthickness=1)
        self.output_preview_frame.place(x=rx+5, y=252, width=rw-10, height=160)
        self.output_preview_label = tk.Label(
            self.output_preview_frame, bg='#eeeeee')
        self.output_preview_label.pack(fill='both', expand=True)

    # ================================================================
    #  文件浏览
    # ================================================================

    def _browse_input(self):
        path = filedialog.askopenfilename(
            title='选择输入视频',
            filetypes=[('视频文件', '*.mp4;*.avi;*.mov;*.mkv;*.wmv'),
                       ('MP4 文件', '*.mp4'),
                       ('所有文件', '*.*')])
        if not path:
            return

        self.input_file = path
        self.input_edit.configure(state='normal')
        self.input_edit.delete(0, 'end')
        self.input_edit.insert(0, path)
        self.input_edit.configure(state='readonly')

        base = os.path.splitext(path)[0]
        self.output_file = base + '.gif'
        self.output_edit.delete(0, 'end')
        self.output_edit.insert(0, self.output_file)

        self._load_video_info()

    def _load_video_info(self):
        try:
            cap = cv2.VideoCapture(self.input_file)
            if not cap.isOpened():
                raise Exception('无法打开视频')

            w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
            h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
            fps = cap.get(cv2.CAP_PROP_FPS)
            total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

            if fps <= 0:
                fps = 30.0
            duration = total_frames / fps if fps > 0 else 0.0

            self.video_info = {
                'width': w, 'height': h,
                'duration': duration, 'fps': fps
            }

            self.video_info_label.configure(
                text=f'视频: {w}×{h} | {duration:.1f} 秒 | '
                     f'{fps:.1f} fps | ~{total_frames} 帧',
                fg='black')

            ret, frame = cap.read()
            if ret:
                self.first_frame_bgr = frame
                self._show_input_preview(frame)

            cap.release()

            if self.preset_var.get() == '原始尺寸':
                self.width_edit.delete(0, 'end')
                self.width_edit.insert(0, str(w))
                self.height_edit.delete(0, 'end')
                self.height_edit.insert(0, str(h))

        except Exception as e:
            self.video_info = {'width': 0, 'height': 0, 'duration': 0.0, 'fps': 0.0}
            self.first_frame_bgr = None
            self.video_info_label.configure(text=f'读取视频失败: {e}', fg='red')
            self._clear_input_preview()

        self._update_size_info()

    def _browse_output(self):
        path = filedialog.asksaveasfilename(
            title='保存 GIF 为',
            defaultextension='.gif',
            filetypes=[('GIF 文件', '*.gif')])
        if path:
            self.output_file = path
            self.output_edit.delete(0, 'end')
            self.output_edit.insert(0, path)

    def _on_output_edited(self, event=None):
        self.output_file = self.output_edit.get()

    # ================================================================
    #  尺寸 / 画质 / 帧率 回调
    # ================================================================

    def _on_preset_changed(self, event=None):
        preset = self.preset_var.get()
        mapping = {
            '微信表情包 (300px)': 300,
            '小尺寸 (200px)': 200,
            '中等 (480px)': 480,
            '大尺寸 (720px)': 720,
        }
        if preset in mapping:
            self._updating_ui = True
            self.ratio_var.set(True)
            self._updating_ui = False
            self.shortest_edit.delete(0, 'end')
            self.shortest_edit.insert(0, str(mapping[preset]))
            self._toggle_ratio_ui(True)
        elif preset == '原始尺寸':
            self._updating_ui = True
            self.ratio_var.set(False)
            self._updating_ui = False
            self._toggle_ratio_ui(False)
            vi = self.video_info
            if vi['width'] > 0:
                self.width_edit.delete(0, 'end')
                self.width_edit.insert(0, str(vi['width']))
                self.height_edit.delete(0, 'end')
                self.height_edit.insert(0, str(vi['height']))
        self._update_size_info()

    def _on_ratio_changed(self):
        if getattr(self, '_updating_ui', False):
            return
        self._toggle_ratio_ui(self.ratio_var.get())
        self.preset_var.set('自定义')
        self._update_size_info()

    def _toggle_ratio_ui(self, maintain):
        if maintain:
            self.frame_manual.pack_forget()
            self.frame_ratio.pack(fill='x')
        else:
            self.frame_ratio.pack_forget()
            self.frame_manual.pack(fill='x')

    def _on_shortest_changed(self, event=None):
        self.preset_var.set('自定义')
        self._update_size_info()

    def _on_manual_size_changed(self, event=None):
        self._update_size_info()

    def _on_quality_fps_changed(self, event=None):
        self._update_size_info()

    # ================================================================
    #  尺寸 & 大小预估
    # ================================================================

    def _get_output_dimensions(self):
        vi = self.video_info
        if vi['width'] == 0:
            return None, None

        if self.ratio_var.get():
            try:
                ss = int(self.shortest_edit.get())
                if ss <= 0:
                    return None, None
            except ValueError:
                return None, None

            if vi['width'] <= vi['height']:
                ow = ss
                oh = round(vi['height'] * (ss / vi['width']))
            else:
                oh = ss
                ow = round(vi['width'] * (ss / vi['height']))
        else:
            try:
                ow = int(self.width_edit.get())
                oh = int(self.height_edit.get())
                if ow <= 0 or oh <= 0:
                    return None, None
            except ValueError:
                return None, None

        return ow, oh

    def _update_size_info(self):
        ow, oh = self._get_output_dimensions()
        if ow is None:
            self.size_info_label.configure(
                text='输出尺寸: -- × --    预计大小: --', fg='#1a1aa6')
            return

        try:
            target_fps = float(self.fps_edit.get())
            if target_fps <= 0:
                target_fps = 15
        except ValueError:
            target_fps = 15

        total_frames = self.video_info['duration'] * target_fps

        comp_ratios = {'最佳': 5.0, '高': 6.5, '中等': 8.5, '低': 11.0}
        comp_ratio = comp_ratios.get(self.quality_var.get(), 5.0)

        est_bytes = ow * oh * total_frames / comp_ratio
        est_str = self._format_size(est_bytes)

        self.size_info_label.configure(
            text=f'输出尺寸: {ow} × {oh}    预计大小: {est_str}  '
                 f'({round(total_frames)} 帧)',
            fg='#1a1aa6')

    @staticmethod
    def _format_size(bytes_val):
        if bytes_val < 1024:
            return f'{bytes_val:.0f} B'
        elif bytes_val < 1024 * 1024:
            return f'{bytes_val / 1024:.1f} KB'
        else:
            return f'{bytes_val / (1024 * 1024):.2f} MB'

    # ================================================================
    #  预览
    # ================================================================

    def _show_input_preview(self, frame_bgr):
        img = self._bgr_to_preview(frame_bgr, 215, 160)
        if img is None:
            return
        self._input_preview_tk = ImageTk.PhotoImage(img)
        self.input_preview_label.configure(
            image=self._input_preview_tk, bg='black')

    def _clear_input_preview(self):
        self.input_preview_label.configure(image='', bg='#eeeeee')

    def _show_output_preview(self, frame_bgr):
        img = self._bgr_to_preview(frame_bgr, 215, 160)
        if img is None:
            return
        self._output_preview_tk = ImageTk.PhotoImage(img)
        self.output_preview_label.configure(
            image=self._output_preview_tk, bg='black')

    def _bgr_to_preview(self, frame_bgr, max_w, max_h):
        """OpenCV BGR → 缩放到预览尺寸的 PIL Image"""
        if frame_bgr is None:
            return None
        h, w = frame_bgr.shape[:2]
        scale = min(max_w / w, max_h / h)
        nw, nh = int(w * scale), int(h * scale)
        resized = cv2.resize(frame_bgr, (nw, nh), interpolation=cv2.INTER_AREA)
        rgb = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)
        return Image.fromarray(rgb)

    # ================================================================
    #  转换
    # ================================================================

    def _start_conversion(self):
        if self.converting:
            return

        if not self.input_file or not os.path.isfile(self.input_file):
            messagebox.showerror('输入错误', '请选择有效的输入视频文件。')
            return
        if not self.output_file:
            messagebox.showerror('输出错误', '请指定输出 GIF 文件。')
            return

        try:
            target_fps = float(self.fps_edit.get())
            if target_fps <= 0 or target_fps > 60:
                raise ValueError
        except ValueError:
            messagebox.showerror('参数错误', '帧率必须在 1 到 60 fps 之间。')
            return

        ow, oh = self._get_output_dimensions()
        if ow is None:
            messagebox.showerror('参数错误', '请设置有效的输出尺寸。')
            return

        q_map = {'最佳': (256, True), '高': (256, False),
                 '中等': (128, False), '低': (64, False)}
        num_colors, use_dither = q_map.get(self.quality_var.get(), (256, True))

        self.converting = True
        self.convert_btn.configure(text='转换中...', state='disabled')
        self.status_label.configure(text='正在启动...', fg='black')
        self.progress_bar['value'] = 0
        self._clear_output_preview()
        self.output_preview_title.configure(text='输出GIF (转换中...)')
        self.root.update_idletasks()

        thread = threading.Thread(
            target=self._do_convert,
            args=(ow, oh, target_fps, num_colors, use_dither),
            daemon=True)
        thread.start()

    def _clear_output_preview(self):
        self.output_preview_label.configure(image='', bg='#eeeeee')

    def _do_convert(self, out_w, out_h, target_fps, num_colors, use_dither):
        try:
            cap = cv2.VideoCapture(self.input_file)
            if not cap.isOpened():
                raise Exception('无法打开视频')

            src_fps = cap.get(cv2.CAP_PROP_FPS)
            if src_fps <= 0:
                src_fps = 30.0
            total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
            if total_frames <= 0 and self.video_info['duration'] > 0:
                total_frames = int(self.video_info['duration'] * src_fps)

            skip_interval = max(1, round(src_fps / target_fps))
            est_output_frames = max(1, int(total_frames / skip_interval))

            self.root.after(0, self._update_status,
                            f'转换中... {out_w}×{out_h}, {num_colors}色, '
                            f'{target_fps:.1f} fps')

            pil_frames = []
            frame_idx = 0

            while True:
                ret, frame = cap.read()
                if not ret:
                    break

                if frame_idx % skip_interval != 0:
                    frame_idx += 1
                    continue

                # BGR → RGB → PIL
                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                # 缩放
                resized = cv2.resize(rgb, (out_w, out_h),
                                     interpolation=cv2.INTER_AREA)
                pil_img = Image.fromarray(resized)

                if use_dither:
                    pil_img = pil_img.quantize(num_colors)
                else:
                    try:
                        pil_img = pil_img.quantize(
                            num_colors, dither=Image.Dither.NONE)
                    except AttributeError:
                        pil_img = pil_img.quantize(num_colors)

                pil_frames.append(pil_img)
                written = len(pil_frames)
                frame_idx += 1

                if written % 5 == 0:
                    pct = min(written / est_output_frames, 1.0)
                    self.root.after(0, self._set_progress, pct)
                    self.root.after(
                        0, self._update_status,
                        f'进度: {written} / ~{est_output_frames} 帧 '
                        f'({pct*100:.0f}%)')

            cap.release()

            if not pil_frames:
                raise Exception('未能从视频中读取任何帧。')

            out_dir = os.path.dirname(os.path.abspath(self.output_file))
            if out_dir:
                os.makedirs(out_dir, exist_ok=True)

            delay_ms = int(1000 / target_fps)
            pil_frames[0].save(
                self.output_file,
                save_all=True,
                append_images=pil_frames[1:],
                duration=delay_ms,
                loop=0,
                optimize=True)

            written = len(pil_frames)
            actual_str = self._format_size(
                os.path.getsize(self.output_file))
            out_name = os.path.basename(self.output_file)

            self.root.after(0, self._set_progress, 1.0)
            self.root.after(
                0, self._conversion_done,
                written, out_w, out_h, target_fps, actual_str, out_name)

        except Exception as e:
            self.root.after(0, self._conversion_error, str(e))

    def _set_progress(self, value):
        self.progress_bar['value'] = value * 100

    def _update_status(self, text):
        self.status_label.configure(text=text)

    def _conversion_done(self, written, out_w, out_h, target_fps,
                         actual_str, out_name):
        self.status_label.configure(
            text=f'完成! {written} 帧, {out_w}×{out_h}, {target_fps:.1f} fps, '
                 f'文件大小: {actual_str}  →  {out_name}',
            fg='#006600')

        if self.first_frame_bgr is not None:
            preview = cv2.resize(self.first_frame_bgr, (out_w, out_h))
            self._show_output_preview(preview)
        self.output_preview_title.configure(text='输出GIF ✓')

        self.size_info_label.configure(
            text=f'输出尺寸: {out_w} × {out_h}    实际大小: {actual_str}  '
                 f'({written} 帧)',
            fg='#006600')

        self.convert_btn.configure(text='开始转换', state='normal')
        self.converting = False

    def _conversion_error(self, msg):
        self.status_label.configure(text=f'错误: {msg}', fg='red')
        self.progress_bar['value'] = 0
        self.output_preview_title.configure(text='输出GIF (失败)')
        self.convert_btn.configure(text='开始转换', state='normal')
        self.converting = False

    def run(self):
        self.root.mainloop()


def _darken(hex_color):
    r, g, b = int(hex_color[1:3], 16), int(hex_color[3:5], 16), int(hex_color[5:7], 16)
    r, g, b = max(0, r - 30), max(0, g - 30), max(0, b - 30)
    return f'#{r:02x}{g:02x}{b:02x}'


if __name__ == '__main__':
    app = MP4ToGifConverter()
    app.run()
