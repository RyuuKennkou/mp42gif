% build_exe.m
% 将 MATLAB GUI 版本打包为独立 exe
% 前提: 已安装 MATLAB Compiler 工具箱

outputDir = fullfile(fileparts(mfilename('fullpath')), 'output');
if ~exist(outputDir, 'dir'), mkdir(outputDir); end

disp('正在编译 mp4_to_gif_gui.exe ...');
mcc -m mp4_to_gif_gui.m -o mp4_to_gif_gui -d output -v

disp(['编译完成! 文件在: ' fullfile(outputDir, 'mp4_to_gif_gui.exe')]);
disp('注意: 目标电脑需安装 MATLAB Runtime (免费, ~2GB)');
