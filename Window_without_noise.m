%%  
% This program is used to select special areas from given camera lines' 
% data and remove the data small than 2*noise_level, then do a Fourier transform for
% all remainning data 
% Input files: xx.csv 
% Output file: XX.fig
% Author: Xinghua Liu
% Data: Sep-18-18
% 
%% Clear all varabels and close all windows
clear all
close all
clc
%% Read data from desk
dn='D:\insect project\data\'; % local address for data
fn=dir([dn '*.csv']); % list all the document with extension of .csv
[~,ind]=sort([fn.datenum]);
D = [];
num_line = length(ind);
% read the first line to determin num of Frame
d=csvread([dn fn(ind(1)).name]); 
num_frame = size(d,1);

center_column = input('Please input the center column');
num_column = input('Please input the number of column');
left_limit = center_column - round(num_column/2)+1;
right_limit = center_column +num_column- round(num_column/2);
D = zeros(num_line,num_frame,num_column);
for k = 1 : length(ind) 
    d = csvread([dn fn(ind(k)).name],0,left_limit,[0,left_limit, num_frame-1 ,right_limit]); % read data file for one line
    D(k,:,:) =d;
end
disp('Data read finished')

%% find data above 2*noise_level
% if the whole Frame dosen't have a data above noise level, then delete it 
noise_level = input('Please input noise level');
i = 1; k = 0; T_delete = [];
% use k to record the num of frame deleted
% use T_delete to record
while(i<num_frame) 
     non_noise = find(D(:,i,:)>2*noise_level);
     if(isempty(non_noise)) % no singal for whole frame, delete this frame
         D(:,i,:) = [];
         i = i -1;
         num_frame = num_frame -1;
         k = k+1;
         T_delete = [T_delete i+k];
     end
     i = i + 1;
end
sampleRate=input('Please input sampleRate');

%% FFT for the remainning data
fs=sampleRate; %Sampling frequency
T = 1/fs;      %Sampling peroid
N=200;
t = (0:num_frame-1)*T; %time vector
f = fs*(0:num_frame/2)/num_frame;  %frequency vector
W=normpdf(1:N,N/2,N/(2*pi));
F_line = length(D(1,:,1));
k = 1; % indicate the # of plot
h = figure;
for i = 1 : num_line
    for j = 1 : num_column
        S = D(i,:,j);
        F = fft(S);
        F_line = F_line +abs(F/num_frame);
    end
    if (k == 6) % every five line in a picture
        h = [h figure()]; % figure to creat new figure window
        k = 1;
    end
    F_line = F_line ./ num_column;    
    subplot(5,1,k);
    plot(f,F_line(1:num_frame/2+1));
    title(['line ',num2str(i)]);
    xlabel('f(HZ)');
    ylabel('|P(f)|');
     k = k+1;
end
savefig(h,[dn 'FFT.fig']);