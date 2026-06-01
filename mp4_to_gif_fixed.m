% mp4_to_gif_fixed.m
% 将 MP4 视频转换为 GIF，最短边设为 300 像素，保持比例

% ===== 用户配置区域 =====
inputVideoFile = '10.mp4';   % 替换为你的 MP4 文件路径
outputGifFile  = '10.gif';  % 输出 GIF 文件名
frameRate      = 15;            % GIF 帧率（例如 10 fps → 每帧延迟 0.1 秒）
% =========================

if ~isfile(inputVideoFile)
    error('输入文件 "%s" 不存在，请检查路径。', inputVideoFile);
end

v = VideoReader(inputVideoFile);
origHeight = v.Height;
origWidth  = v.Width;

% 计算新尺寸：最短边为 300，保持比例
if origWidth < origHeight
    newWidth  = 720;
    newHeight = round(origHeight * (300 / origWidth));
else
    newHeight = 720;
    newWidth  = round(origWidth * (300 / origHeight));
end

fprintf('原始尺寸: %d x %d → 新尺寸: %d x %d\n', origWidth, origHeight, newWidth, newHeight);

% 计算帧间隔（跳帧可减小文件大小，可选）
totalFrames = floor(v.FrameRate * v.Duration);
targetInterval = max(1, round(v.FrameRate / frameRate)); % 例如原视频30fps，目标10fps → 每3帧取1帧
fprintf('原帧率: %.2f fps，目标帧率: %d fps（每 %d 帧取1帧）\n', v.FrameRate, frameRate, targetInterval);

% 重置视频读取器
v.CurrentTime = 0;

% 写入第一帧（必须先写一帧，不使用 'Append'）
frameCount = 0;
while hasFrame(v)
    frame = readFrame(v);
    
    % 跳帧控制
    if mod(frameCount, targetInterval) ~= 0
        frameCount = frameCount + 1;
        continue;
    end
    
    resizedFrame = imresize(frame, [newHeight, newWidth]);
    
    % 将 RGB 转为索引图像（GIF 要求）
    [indImg, map] = rgb2ind(resizedFrame, 256);
    
    if frameCount == 0
        % 写入第一帧
        imwrite(indImg, map, outputGifFile, ...
            'DelayTime', 1/frameRate, ...
            'LoopCount', inf);
    else
        % 追加后续帧
        imwrite(indImg, map, outputGifFile, ...
            'DelayTime', 1/frameRate, ...
            'WriteMode', 'append');
    end
    
    frameCount = frameCount + 1;
end

fprintf('GIF 生成完成！共写入 %d 帧，帧率 ≈ %d fps\n', floor(frameCount / targetInterval), frameRate);