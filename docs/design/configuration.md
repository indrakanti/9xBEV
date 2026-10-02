# YAML configuration v1

Use `config/qualcomm_linux.yaml` as the complete example. Every shown field is required; unknown and duplicate keys fail. Paths are relative to the YAML directory. Startup configuration only; hot reload is pending. YAML must be trusted deployment input. The sample geometry is synthetic and is not a calibration for a real car.

| Section | Fields and units |
|---|---|
| renderer | backend: cpu/opengl/cuda; output PPM path; width/height pixels; CUDA device index and block size |
| vehicle | length_m/width_m; normalized RGB footprint color |
| bowl | flat_radius_m/outer_radius_m/height_m; rings/sectors tessellation |
| view | yaw_rad, tilt_rad (0 = top view, increasing toward side); extent_m vertical half-span; orthographic viewpoint |
| blend | angular weight_power; positive min_weight; normalized RGB background |
| cameras | id, input PPM path, resolution [width,height], position_m [x,y,z], row-major R_vehicle_camera [9], intrinsics [fx,fy,cx,cy], distortion [k1,k2,k3,k4], max_theta_rad and RGB gain |

Vehicle frame X forward, Y left, Z up; origin is the vehicle center at nominal ground. Optical frame X right, Y down, Z forward. Right-handed active rotations: p_vehicle = R_vehicle_camera p_camera + position_m. Intrinsics use pixels; image origin top-left, integer coordinates address pixel centers. Distortion uses theta_d = theta (1+k1 theta²+k2 theta⁴+k3 theta⁶+k4 theta⁸). max_theta is the usable lens half-angle, additionally bounded by image dimensions. Distortion derivative is sampled for positivity; this is not a formal proof of monotonicity between samples.

Color blending is in encoded RGB in this milestone; linear-light blending, ISP exposure matching and NV12 conversion are pending. The bowl is a projection surface, not a reconstructed environment: upright objects can distort or duplicate at seams. Gain is a manually configured multiplier, not automatic exposure balancing. Footprint is rectangular, not a vehicle CAD model.
