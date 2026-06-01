% build_exe.m
% 将 MP4 → GIF 转换器打包为独立 exe
% 前提: 已安装 MATLAB Compiler 工具箱
%
% 运行方式: 在 MATLAB 命令行输入 build_exe

outputDir = fullfile(fileparts(mfilename('fullpath')), 'output');
if ~exist(outputDir, 'dir')
    mkdir(outputDir);
end

disp('正在编译 mp4_to_gif_gui.exe ...');
mcc -m mp4_to_gif_gui.m ...
    -o mp4_to_gif_gui ...
    -d output ...
    -v

disp(['编译完成! 文件在: ' fullfile(outputDir, 'mp4_to_gif_gui.exe')]);
