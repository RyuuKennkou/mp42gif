% mp4_to_gif_fixed.m
% 将 MP4 视频转换为 GIF，最短边设为指定像素，保持比例
% 直接运行此脚本，按照提示修改 inputVideoFile 即可

% ===== 用户配置区域 =====
inputVideoFile = 'input.mp4';   % 替换为你的 MP4 文件路径
outputGifFile  = 'output.gif';  % 输出 GIF 文件名
shortestSide   = 300;           % 最短边像素 (微信表情包推荐 300)
frameRate      = 15;            % GIF 帧率
quality        = 'best';        % 'best' | 'high' | 'medium' | 'low'
% =========================

if ~isfile(inputVideoFile)
    error('输入文件 "%s" 不存在，请检查路径。', inputVideoFile);
end

v = VideoReader(inputFile);
origHeight = v.Height;
origWidth  = v.Width;

% 计算新尺寸：最短边为 shortestSide，保持比例
if origWidth < origHeight
    newWidth  = shortestSide;
    newHeight = round(origHeight * (shortestSide / origWidth));
else
    newHeight = shortestSide;
    newWidth  = round(origWidth * (shortestSide / origHeight));
end

fprintf('原始尺寸: %d x %d -> 新尺寸: %d x %d\n', origWidth, origHeight, newWidth, newHeight);

% 画质参数
switch lower(quality)
    case 'best',   numColors = 256; useDither = true;
    case 'high',   numColors = 256; useDither = false;
    case 'medium', numColors = 128; useDither = false;
    case 'low',    numColors = 64;  useDither = false;
    otherwise,     numColors = 256; useDither = true;
end

% 计算帧间隔
totalFrames = floor(v.FrameRate * v.Duration);
targetInterval = max(1, round(v.FrameRate / frameRate));
fprintf('原帧率: %.2f fps，目标帧率: %d fps（每 %d 帧取1帧）\n', v.FrameRate, frameRate, targetInterval);

v.CurrentTime = 0;
frameCount = 0;
writtenFrames = 0;

while hasFrame(v)
    frame = readFrame(v);

    if mod(frameCount, targetInterval) ~= 0
        frameCount = frameCount + 1;
        continue;
    end

    resizedFrame = imresize(frame, [newHeight, newWidth]);

    if useDither
        [indImg, map] = rgb2ind(resizedFrame, numColors);
    else
        [indImg, map] = rgb2ind(resizedFrame, numColors, 'nodither');
    end

    if writtenFrames == 0
        imwrite(indImg, map, outputGifFile, ...
            'DelayTime', 1/frameRate, ...
            'LoopCount', inf);
    else
        imwrite(indImg, map, outputGifFile, ...
            'DelayTime', 1/frameRate, ...
            'WriteMode', 'append');
    end

    writtenFrames = writtenFrames + 1;
    frameCount = frameCount + 1;
end

fprintf('GIF 生成完成！共写入 %d 帧，帧率 ~ %d fps\n', writtenFrames, frameRate);
