function mp4_to_gif_gui()
% MP4 转 GIF 转换器 (GUI)

    if ~license('test', 'Image_Toolbox')
        errordlg('需要 Image Processing Toolbox (imresize 函数)。', '缺少工具箱');
        return;
    end

    % ===== 状态变量 =====
    inputFile = '';
    outputFile = '';
    V = struct('width', 0, 'height', 0, 'duration', 0, 'frameRate', 0);
    inputFirstFrame = [];

    % ===== 主窗口 =====
    figW = 800;
    figH = 560;
    fig = figure('Name', 'MP4 → GIF 转换器', ...
                 'NumberTitle', 'off', ...
                 'Position', [300, 180, figW, figH], ...
                 'Resize', 'off', ...
                 'MenuBar', 'none', ...
                 'ToolBar', 'none', ...
                 'Color', 'white');
    set(fig, 'DefaultUicontrolFontName', 'Microsoft YaHei', ...
             'DefaultUicontrolFontSize', 10, ...
             'DefaultUicontrolBackgroundColor', 'white');
    bg = 'white';
    fsn = 10;  % 标准字号

    leftW = 500;
    rightX = 540;
    rightW = 245;
    y0 = figH - 35;  % 525

    % ============ 标题 ============
    uicontrol('Style', 'text', 'String', 'MP4 → GIF 转换器', ...
              'Position', [15, y0, leftW, 30], ...
              'FontSize', 18, 'FontWeight', 'bold', ...
              'HorizontalAlignment', 'center', 'BackgroundColor', bg);

    y0 = y0 - 54;  % 471

    % ============ 输入文件 ============
    uicontrol('Style', 'text', 'String', '输入视频:', ...
              'Position', [15, y0+23, 70, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, 'FontSize', fsn);
    hInputEdit = uicontrol('Style', 'edit', 'String', '', ...
              'Position', [15, y0, 380, 25], ...
              'HorizontalAlignment', 'left', 'Enable', 'inactive', ...
              'BackgroundColor', 'white', 'FontSize', fsn);
    uicontrol('Style', 'pushbutton', 'String', '浏览...', ...
              'Position', [405, y0, 100, 26], ...
              'FontSize', fsn, 'Callback', @cbBrowseInput);

    y0 = y0 - 50;  % 431

    % ============ 输出文件 ============
    uicontrol('Style', 'text', 'String', '输出GIF:', ...
              'Position', [15, y0+23, 70, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, 'FontSize', fsn);
    hOutputEdit = uicontrol('Style', 'edit', 'String', '', ...
              'Position', [15, y0, 380, 25], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', 'white', ...
              'FontSize', fsn, 'Callback', @cbOutputEdited);
    uicontrol('Style', 'pushbutton', 'String', '浏览...', ...
              'Position', [405, y0, 100, 26], ...
              'FontSize', fsn, 'Callback', @cbBrowseOutput);

    y0 = y0 - 30;  % 401

    % ============ 视频信息 ============
    hVideoInfo = uicontrol('Style', 'text', 'String', '未加载视频', ...
              'Position', [15, y0, leftW, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, ...
              'ForegroundColor', [0.5 0.5 0.5], 'FontSize', fsn);

    % ---- 分隔线 ----
    y0 = y0 - 30;  % 371
    uicontrol('Style', 'text', 'String', repmat('─', 1, 60), ...
              'Position', [15, y0, leftW, 10], ...
              'BackgroundColor', bg, 'ForegroundColor', [0.7 0.7 0.7]);

    y0 = y0 - 26;  % 345

    % ============ 尺寸设置 ============
    uicontrol('Style', 'text', 'String', '尺寸设置', ...
              'Position', [15, y0+20, 80, 20], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, ...
              'FontSize', 12, 'FontWeight', 'bold');

    y0 = y0 - 36;  % 309

    uicontrol('Style', 'text', 'String', '预设:', ...
              'Position', [15, y0+5, 40, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, 'FontSize', fsn);
    hPreset = uicontrol('Style', 'popupmenu', ...
              'String', {'微信表情包 (300px)', '小尺寸 (200px)', '中等 (480px)', ...
                         '大尺寸 (720px)', '原始尺寸', '自定义'}, ...
              'Position', [55, y0, 185, 25], ...
              'Value', 1, 'BackgroundColor', 'white', 'FontSize', fsn, ...
              'Callback', @cbPresetChanged);

    y0 = y0 - 38;  % 271

    hRatioCheck = uicontrol('Style', 'checkbox', ...
              'String', '保持原始宽高比 (仅需设置最短边)', ...
              'Position', [15, y0, 300, 20], ...
              'Value', 1, 'BackgroundColor', bg, 'FontSize', fsn, ...
              'Callback', @cbRatioChanged);

    y0 = y0 - 34;  % 237

    % ---- 最短边 / 宽高 ----
    hLabelSS = uicontrol('Style', 'text', 'String', '最短边:', ...
              'Position', [32, y0+5, 55, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, 'FontSize', fsn);
    hShortestSide = uicontrol('Style', 'edit', 'String', '300', ...
              'Position', [90, y0, 58, 25], ...
              'HorizontalAlignment', 'center', 'BackgroundColor', 'white', ...
              'FontSize', fsn, 'Callback', @cbShortestSideChanged);
    hLabelSSpx = uicontrol('Style', 'text', 'String', '像素', ...
              'Position', [152, y0+5, 35, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, 'FontSize', fsn);

    hLabelW = uicontrol('Style', 'text', 'String', '宽度:', ...
              'Position', [32, y0+5, 42, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, ...
              'FontSize', fsn, 'Visible', 'off');
    hWidthEdit = uicontrol('Style', 'edit', 'String', '', ...
              'Position', [74, y0, 60, 25], ...
              'HorizontalAlignment', 'center', 'BackgroundColor', 'white', ...
              'FontSize', fsn, 'Visible', 'off', 'Callback', @cbManualSizeChanged);
    hLabelH = uicontrol('Style', 'text', 'String', '高度:', ...
              'Position', [142, y0+5, 42, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, ...
              'FontSize', fsn, 'Visible', 'off');
    hHeightEdit = uicontrol('Style', 'edit', 'String', '', ...
              'Position', [184, y0, 60, 25], ...
              'HorizontalAlignment', 'center', 'BackgroundColor', 'white', ...
              'FontSize', fsn, 'Visible', 'off', 'Callback', @cbManualSizeChanged);
    hLabelWHpx = uicontrol('Style', 'text', 'String', '像素', ...
              'Position', [248, y0+5, 35, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, ...
              'FontSize', fsn, 'Visible', 'off');

    % ---- 输出尺寸与预估大小 ----
    y0 = y0 - 32;  % 205
    hSizeInfo = uicontrol('Style', 'text', ...
              'String', '输出尺寸: -- × --    预计大小: --', ...
              'Position', [15, y0, leftW, 22], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, ...
              'FontSize', fsn, 'FontWeight', 'bold', ...
              'ForegroundColor', [0.1 0.1 0.65]);

    % ---- 分隔线 ----
    y0 = y0 - 32;  % 173
    uicontrol('Style', 'text', 'String', repmat('─', 1, 60), ...
              'Position', [15, y0, leftW, 10], ...
              'BackgroundColor', bg, 'ForegroundColor', [0.7 0.7 0.7]);

    y0 = y0 - 26;  % 147

    % ============ 画质 & 帧率 ============
    uicontrol('Style', 'text', 'String', '画质:', ...
              'Position', [15, y0+5, 40, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, 'FontSize', fsn);
    hQualityMenu = uicontrol('Style', 'popupmenu', ...
              'String', {'最佳', '高', '中等', '低'}, ...
              'Position', [55, y0, 100, 25], ...
              'Value', 1, 'BackgroundColor', 'white', 'FontSize', fsn, ...
              'Callback', @cbQualityOrFpsChanged);

    uicontrol('Style', 'text', 'String', '帧率:', ...
              'Position', [180, y0+5, 40, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, 'FontSize', fsn);
    hFpsEdit = uicontrol('Style', 'edit', 'String', '15', ...
              'Position', [222, y0, 50, 25], ...
              'HorizontalAlignment', 'center', 'BackgroundColor', 'white', ...
              'FontSize', fsn, 'Callback', @cbQualityOrFpsChanged);
    uicontrol('Style', 'text', 'String', 'fps', ...
              'Position', [276, y0+5, 25, 18], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, 'FontSize', fsn);

    uicontrol('Style', 'text', ...
              'String', '最佳=256色+抖动 | 高=256色 | 中=128色 | 低=64色', ...
              'Position', [15, y0-18, 350, 16], ...
              'HorizontalAlignment', 'left', 'BackgroundColor', bg, ...
              'ForegroundColor', [0.55 0.55 0.55], 'FontSize', 8);

    % ============ 转换按钮 ============
    y0 = y0 - 55;  % 92
    hConvertBtn = uicontrol('Style', 'pushbutton', 'String', '开始转换', ...
              'Position', [120, y0, 260, 40], ...
              'FontSize', 14, 'FontWeight', 'bold', ...
              'BackgroundColor', [0.15 0.55 0.15], ...
              'ForegroundColor', 'white', ...
              'Callback', @cbStartConversion);

    % ============ 进度条 ============
    y0 = y0 - 46;  % 46
    hProgBg = uicontrol('Style', 'text', ...
              'Position', [15, y0, leftW, 14], ...
              'BackgroundColor', [0.88 0.88 0.88], ...
              'Visible', 'off');
    hProgBar = uicontrol('Style', 'text', ...
              'Position', [15, y0, 0, 14], ...
              'BackgroundColor', [0.15 0.55 0.15], ...
              'Visible', 'off');

    % ============ 状态 ============
    y0 = y0 - 24;  % 22
    hStatus = uicontrol('Style', 'text', 'String', '就绪', ...
              'Position', [15, y0, leftW, 20], ...
              'HorizontalAlignment', 'center', 'BackgroundColor', bg, ...
              'ForegroundColor', [0.4 0.4 0.4], 'FontSize', fsn);

    % ================================================================
    %  右侧: 预览面板
    % ================================================================

    uicontrol('Style', 'text', 'String', '输入视频', ...
              'Position', [rightX, figH-35, rightW, 22], ...
              'HorizontalAlignment', 'center', 'BackgroundColor', bg, ...
              'FontSize', fsn, 'FontWeight', 'bold');
    axInput = axes('Units', 'pixels', ...
              'Position', [rightX+10, figH-215, rightW-20, 160], ...
              'XTick', [], 'YTick', [], ...
              'Box', 'on', 'Color', [0.93 0.93 0.93]);

    uicontrol('Style', 'text', 'String', '↓', ...
              'Position', [rightX, figH-240, rightW, 16], ...
              'HorizontalAlignment', 'center', 'BackgroundColor', bg, ...
              'ForegroundColor', [0.55 0.55 0.55], 'FontSize', 12);

    hOutputPrevLabel = uicontrol('Style', 'text', 'String', '输出GIF', ...
              'Position', [rightX, figH-262, rightW, 22], ...
              'HorizontalAlignment', 'center', 'BackgroundColor', bg, ...
              'FontSize', fsn, 'FontWeight', 'bold');
    axOutput = axes('Units', 'pixels', ...
              'Position', [rightX+10, figH-440, rightW-20, 160], ...
              'XTick', [], 'YTick', [], ...
              'Box', 'on', 'Color', [0.93 0.93 0.93]);

    % ================================================================
    %  回调函数
    % ================================================================

    function cbBrowseInput(~, ~)
        [file, path] = uigetfile(...
            {'*.mp4;*.avi;*.mov;*.mkv;*.wmv', '视频文件 (*.mp4, *.avi, *.mov, *.mkv, *.wmv)'; ...
             '*.mp4', 'MP4 文件 (*.mp4)'; ...
             '*.*', '所有文件 (*.*)'}, ...
            '选择输入视频');
        if isequal(file, 0), return; end

        inputFile = fullfile(path, file);
        set(hInputEdit, 'String', inputFile);

        [~, name, ~] = fileparts(inputFile);
        outputFile = fullfile(path, [name, '.gif']);
        set(hOutputEdit, 'String', outputFile);

        try
            v = VideoReader(inputFile);
            V.width = v.Width;
            V.height = v.Height;
            V.duration = v.Duration;
            V.frameRate = v.FrameRate;
            nFrames = floor(v.FrameRate * v.Duration);
            set(hVideoInfo, 'String', ...
                sprintf('视频: %d×%d | %.1f 秒 | %.1f fps | ~%d 帧', ...
                        V.width, V.height, V.duration, V.frameRate, nFrames), ...
                'ForegroundColor', [0 0 0]);

            if get(hPreset, 'Value') == 5
                set(hWidthEdit, 'String', num2str(V.width));
                set(hHeightEdit, 'String', num2str(V.height));
            end

            v.CurrentTime = 0;
            if hasFrame(v)
                inputFirstFrame = readFrame(v);
                showInputPreview(inputFirstFrame);
            end
        catch
            V = struct('width', 0, 'height', 0, 'duration', 0, 'frameRate', 0);
            inputFirstFrame = [];
            set(hVideoInfo, 'String', '读取视频失败', 'ForegroundColor', 'red');
            clearInputPreview();
        end
        updateSizeInfo();
    end

    function cbBrowseOutput(~, ~)
        [file, path] = uiputfile({'*.gif', 'GIF 文件 (*.gif)'}, '保存 GIF 为');
        if isequal(file, 0), return; end
        outputFile = fullfile(path, file);
        set(hOutputEdit, 'String', outputFile);
    end

    function cbOutputEdited(~, ~)
        outputFile = get(hOutputEdit, 'String');
    end

    function cbPresetChanged(~, ~)
        val = get(hPreset, 'Value');
        switch val
            case 1
                set(hRatioCheck, 'Value', 1);
                set(hShortestSide, 'String', '300');
                toggleRatioUI(true);
            case 2
                set(hRatioCheck, 'Value', 1);
                set(hShortestSide, 'String', '200');
                toggleRatioUI(true);
            case 3
                set(hRatioCheck, 'Value', 1);
                set(hShortestSide, 'String', '480');
                toggleRatioUI(true);
            case 4
                set(hRatioCheck, 'Value', 1);
                set(hShortestSide, 'String', '720');
                toggleRatioUI(true);
            case 5
                set(hRatioCheck, 'Value', 0);
                toggleRatioUI(false);
                if V.width > 0
                    set(hWidthEdit, 'String', num2str(V.width));
                    set(hHeightEdit, 'String', num2str(V.height));
                end
        end
        updateSizeInfo();
    end

    function cbRatioChanged(~, ~)
        if get(hRatioCheck, 'Value')
            toggleRatioUI(true);
        else
            toggleRatioUI(false);
        end
        set(hPreset, 'Value', 6);
        updateSizeInfo();
    end

    function toggleRatioUI(maintainRatio)
        if maintainRatio
            set(hLabelSS,    'Visible', 'on');
            set(hShortestSide, 'Visible', 'on');
            set(hLabelSSpx,  'Visible', 'on');
            set(hLabelW,     'Visible', 'off');
            set(hWidthEdit,  'Visible', 'off');
            set(hLabelH,     'Visible', 'off');
            set(hHeightEdit, 'Visible', 'off');
            set(hLabelWHpx,  'Visible', 'off');
        else
            set(hLabelSS,    'Visible', 'off');
            set(hShortestSide, 'Visible', 'off');
            set(hLabelSSpx,  'Visible', 'off');
            set(hLabelW,     'Visible', 'on');
            set(hWidthEdit,  'Visible', 'on');
            set(hLabelH,     'Visible', 'on');
            set(hHeightEdit, 'Visible', 'on');
            set(hLabelWHpx,  'Visible', 'on');
        end
    end

    function cbShortestSideChanged(~, ~)
        set(hPreset, 'Value', 6);
        updateSizeInfo();
    end

    function cbManualSizeChanged(~, ~)
        updateSizeInfo();
    end

    function cbQualityOrFpsChanged(~, ~)
        updateSizeInfo();
    end

    function updateSizeInfo()
        if V.width == 0
            set(hSizeInfo, 'String', '输出尺寸: -- × --    预计大小: --');
            return;
        end

        valid = true;
        if get(hRatioCheck, 'Value')
            ss = str2double(get(hShortestSide, 'String'));
            if isnan(ss) || ss <= 0
                valid = false;
            else
                if V.width <= V.height
                    outW = round(ss);
                    outH = round(V.height * (ss / V.width));
                else
                    outH = round(ss);
                    outW = round(V.width * (ss / V.height));
                end
            end
        else
            outW = str2double(get(hWidthEdit, 'String'));
            outH = str2double(get(hHeightEdit, 'String'));
            if isnan(outW) || isnan(outH) || outW <= 0 || outH <= 0
                valid = false;
            else
                outW = round(outW);
                outH = round(outH);
            end
        end

        if ~valid
            set(hSizeInfo, 'String', '输出尺寸: (输入无效)    预计大小: --');
            return;
        end

        targetFPS = str2double(get(hFpsEdit, 'String'));
        if isnan(targetFPS) || targetFPS <= 0, targetFPS = 15; end
        totalOutFrames = V.duration * targetFPS;

        qIdx = get(hQualityMenu, 'Value');
        switch qIdx
            case 1, compRatio = 5.0;
            case 2, compRatio = 6.5;
            case 3, compRatio = 8.5;
            case 4, compRatio = 11.0;
        end
        estBytes = outW * outH * totalOutFrames / compRatio;
        estSizeStr = formatSize(estBytes);

        set(hSizeInfo, 'String', ...
            sprintf('输出尺寸: %d × %d    预计大小: %s  (%d 帧)', ...
                    outW, outH, estSizeStr, round(totalOutFrames)));
    end

    function s = formatSize(bytes)
        if bytes < 1024
            s = sprintf('%.0f B', bytes);
        elseif bytes < 1024 * 1024
            s = sprintf('%.1f KB', bytes / 1024);
        else
            s = sprintf('%.2f MB', bytes / (1024 * 1024));
        end
    end

    % ================================================================
    %  预览
    % ================================================================

    function showInputPreview(frame)
        axes(axInput);
        cla;
        imshow(frame, 'Parent', axInput, 'Border', 'tight');
        set(axInput, 'XTick', [], 'YTick', [], 'Box', 'on');
    end

    function clearInputPreview()
        axes(axInput);
        cla;
        set(axInput, 'XTick', [], 'YTick', [], 'Color', [0.93 0.93 0.93]);
    end

    function showOutputPreview(frame)
        axes(axOutput);
        cla;
        imshow(frame, 'Parent', axOutput, 'Border', 'tight');
        set(axOutput, 'XTick', [], 'YTick', [], 'Box', 'on');
        set(hOutputPrevLabel, 'String', '输出GIF ✓');
    end

    % ================================================================
    %  转换
    % ================================================================

    function cbStartConversion(~, ~)
        if isempty(inputFile) || ~isfile(inputFile)
            errordlg('请选择有效的输入视频文件。', '输入错误');
            return;
        end
        if isempty(outputFile)
            errordlg('请指定输出 GIF 文件。', '输出错误');
            return;
        end

        targetFPS = str2double(get(hFpsEdit, 'String'));
        if isnan(targetFPS) || targetFPS <= 0 || targetFPS > 60
            errordlg('帧率必须在 1 到 60 fps 之间。', '参数错误');
            return;
        end

        if get(hRatioCheck, 'Value')
            ss = str2double(get(hShortestSide, 'String'));
            if isnan(ss) || ss <= 0 || ss > 4096
                errordlg('最短边必须在 1 到 4096 像素之间。', '参数错误');
                return;
            end
            if V.width <= V.height
                outW = round(ss);
                outH = round(V.height * (ss / V.width));
            else
                outH = round(ss);
                outW = round(V.width * (ss / V.height));
            end
        else
            outW = str2double(get(hWidthEdit, 'String'));
            outH = str2double(get(hHeightEdit, 'String'));
            if isnan(outW) || isnan(outH) || outW <= 0 || outH <= 0
                errordlg('宽度和高度必须为正整数。', '参数错误');
                return;
            end
            outW = round(outW);
            outH = round(outH);
        end

        qIdx = get(hQualityMenu, 'Value');
        switch qIdx
            case 1, numColors = 256; useDither = true;
            case 2, numColors = 256; useDither = false;
            case 3, numColors = 128; useDither = false;
            case 4, numColors = 64;  useDither = false;
        end

        set(hConvertBtn, 'Enable', 'off', 'String', '转换中...');
        set(hStatus, 'String', '正在启动...', 'ForegroundColor', [0 0 0]);
        set(hProgBg, 'Visible', 'on');
        set(hProgBar, 'Visible', 'on');
        setProg(0);
        axes(axOutput); cla; set(axOutput, 'Color', [0.93 0.93 0.93]);
        set(hOutputPrevLabel, 'String', '输出GIF (转换中...)');
        drawnow;

        try
            v = VideoReader(inputFile);
            srcFPS = v.FrameRate;
            totalFrames = floor(srcFPS * v.Duration);
            skipInterval = max(1, round(srcFPS / targetFPS));
            estOutputFrames = ceil(totalFrames / skipInterval);

            set(hStatus, 'String', ...
                sprintf('转换中... %d×%d, %d色, %.1f fps', ...
                        outW, outH, numColors, targetFPS));
            drawnow;

            v.CurrentTime = 0;
            frameIdx = 0;
            written = 0;

            while hasFrame(v)
                frame = readFrame(v);

                if mod(frameIdx, skipInterval) ~= 0
                    frameIdx = frameIdx + 1;
                    continue;
                end

                resized = imresize(frame, [outH, outW]);

                if useDither
                    [indImg, cmap] = rgb2ind(resized, numColors);
                else
                    [indImg, cmap] = rgb2ind(resized, numColors, 'nodither');
                end

                if written == 0
                    imwrite(indImg, cmap, outputFile, ...
                            'DelayTime', 1/targetFPS, 'LoopCount', inf);
                else
                    imwrite(indImg, cmap, outputFile, ...
                            'DelayTime', 1/targetFPS, 'WriteMode', 'append');
                end

                written = written + 1;
                frameIdx = frameIdx + 1;

                if mod(written, 5) == 0
                    setProg(min(written / estOutputFrames, 1));
                    set(hStatus, 'String', ...
                        sprintf('进度: %d / ~%d 帧 (%.0f%%)', ...
                                written, estOutputFrames, 100*written/estOutputFrames));
                    drawnow;
                end
            end

            setProg(1);

            fInfo = dir(outputFile);
            actualSize = formatSize(fInfo.bytes);

            set(hStatus, 'String', ...
                sprintf('完成! %d 帧, %d×%d, %.1f fps, 文件大小: %s  →  %s', ...
                        written, outW, outH, targetFPS, actualSize, outputFile), ...
                'ForegroundColor', [0 0.4 0]);

            if ~isempty(inputFirstFrame)
                previewFrame = imresize(inputFirstFrame, [outH, outW]);
                showOutputPreview(previewFrame);
            end

            set(hSizeInfo, 'String', ...
                sprintf('输出尺寸: %d × %d    实际大小: %s  (%d 帧)', ...
                        outW, outH, actualSize, written), ...
                'ForegroundColor', [0 0.4 0]);

        catch ME
            set(hStatus, 'String', ['错误: ' ME.message], 'ForegroundColor', 'red');
            set(hProgBg, 'Visible', 'off');
            set(hProgBar, 'Visible', 'off');
            set(hOutputPrevLabel, 'String', '输出GIF (失败)');
        end

        set(hConvertBtn, 'Enable', 'on', 'String', '开始转换');
    end

    function setProg(frac)
        bgPos = get(hProgBg, 'Position');
        w = max(1, round(frac * bgPos(3)));
        set(hProgBar, 'Position', [bgPos(1), bgPos(2), w, bgPos(4)]);
    end

end
