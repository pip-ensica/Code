addpath( '~/Code/PointCloudGenerator/' );
addpath( '~/Code/LevelSetsMethods3D/' );
addpath( '~/Code/filtering/' );
addpath( '~/Code/file_management/' );
addpath( '~/Code/filtering/mahalanobis/' );
clear

dataset = 'BUNNY';

if strcmp( dataset, 'BUNNY' )
    %X = []; Y = []; Z = [];
    fid = fopen( '~/Data/PiFiles/20100204-000000-000.3pi','r' );
    %line_start_char = '#';
    %line = 'not_empty';
    
    %while size(line,2) > 1
    %    line = fgetl(fid);
       % if size(line,2) > 1 && line(1) ~= '#'
    %       A = fscanf( fid, '%f %f %f %d %d', [5 1280] );
    %       X = [ X A(1,:) ];
    %       Y = [ Y A(2,:) ];
    %       Z = [ Z A(3,:) ];
       % end
    %end
    % the above misses the first point
    A = fscanf( fid, '%f %f %f %d %d', [5 inf] );
    X = A(1,:);
    Y = A(2,:);
    Z = A(3,:);
    g = A(4,:);
    radius = 0.00003;
    h = 200;
    h_fixed = 1;
    k = 50;
    n = 10;
elseif strcmp( dataset, 'BUDDHA' )
    [tri,pts] = plyread( '/home/mark/Data/happy_recon/happy_vrip_res2.ply','tri' );
    X = pts(:,1)';
    Y = pts(:,2)';
    Z = pts(:,3)';
    radius = 0.00001;
    n = 10;
    h = 200;
    h_fixed = 1;
elseif strcmp( dataset, 'DRILL' )
    fid = fopen( '~/Data/drill/reconstruction/drill_shaft_vrip_stripped.ply', 'r' );
    A = fscanf( fid, '%f %f %f %f\n', [4 inf] );
    X = A(1,:);
    Y = A(2,:);
    Z = A(3,:);
    radius = 0.01;
elseif strcmp( dataset, 'TORUS' )
    [tri,pts] = plyread( '~/Data/torus.ply','tri' );
    X = pts(:,1)';
    Y = pts(:,2)';
    Z = pts(:,3)';
    radius = 0.05;
    n = 3;
    h_fixed = 1;
    h = 20;
end

N = 100;

x_min = min( X );
x_max = max( X );
y_min = min( Y );
y_max = max( Y );
z_min = min( Z );
z_max = max( Z );

dx = (x_max - x_min) / N;
dy = (y_max - y_min) / N;
dz = (z_max - z_min) / N;

%{
x_min = x_min - 100*dx; 
x_max = x_max + 100*dx;
y_min = y_min - 100*dy; 
y_max = y_max + 100*dy;
z_min = z_min - 100*dz; 
z_max = z_max + 100*dz;
[x,y,z] = meshgrid( x_min:dx:x_max,y_min:dy:y_max,z_min:dz:z_max );
%}

freq = 5;
%[X_noisy,Y_noisy,Z_noisy] = addNoise( X,Y,Z,freq,0.004 );
[X_noisy,Y_noisy,Z_noisy] = addNoisyPoints( X,Y,Z,size(X,2)*4,dx,dy,dz );

X_vec = [ X_noisy' , Y_noisy', Z_noisy' ];
q_x = ones(size(X_noisy));
f = ones(size(X_noisy));
f_fixed_h = ones(size(X_noisy));

q_x_radius = ones(size(X_noisy));
f_radius = ones(size(X_noisy));
f_fixed_h_radius = ones(size(X_noisy));

for i=1:size(X_vec,1)
%for i=[1:50,size(X,2)+1:size(X,2)+51]
   %[q_x(i),id,dist,f(i),f_fixed_h(i)] = kernel_density_estimate( X_vec,i,k,n,h,h_fixed );
   [q_x_radius(i),id,dist,f_radius(i),f_fixed_h_radius(i)] = kernel_density_estimate_within_radius( X_vec,i,radius,n,h,h_fixed );
   if mod(i,200) == 0
       sprintf( '%f completed', i/size(X_vec,1)*100 )
   end
end

set(gcf, 'renderer', 'zbuffer')
plot3( X_noisy,Y_noisy,Z_noisy,'or','markersize',.12)


if strcmp( dataset, 'BUDDHA' )
    [A1] = exportOffFile( X,Y,Z, '~/Data/buddha_prior_to_noise_addition.off'  );
    idx = find( f_radius >= 0.11 );
    [A2] = exportOffFile( X_noisy(idx),Y_noisy(idx),Z_noisy(idx), '~/Data/buddha_filt.off'  );
end

% now check detection algorithm
detection_reference = zeros(size(f));
detection_reference(size(X,2)+1:end) = 1;
%detection_reference(1:freq:end) = 1;

thresh_max = max( f_fixed_h_radius .* detection_reference );

roc_x = [];
roc_y = [];
true_positive_detect = 1;
while true_positive_detect > 0
    detection_result = f_fixed_h_radius < thresh_max;
    thresh_max = thresh_max - 0.0005;
    
    actual_normal_class = ~detection_reference;
    actual_outlier_class = detection_reference;
    
    
    
    true_positive_detect =  sum( detection_reference == 1 & detection_result == 1 );
    false_negative_detect = sum( detection_reference == 1 & detection_result == 0 );   
    
    false_positive_detect =  sum( detection_reference == 0 & detection_result == 1 );
    true_negative_detect  =  sum( detection_reference == 0 & detection_result == 0 );
    
    detect_rate = true_positive_detect / (true_positive_detect + false_negative_detect );
    false_alarm_rate = false_positive_detect / ( false_positive_detect + true_negative_detect );
    roc_x = [roc_x detect_rate];
    roc_y = [roc_y false_alarm_rate];
end
