#version 460

layout(rgba32f, binding = 0) uniform image2D inputImage;
layout(rgba32f, binding = 1) uniform image2D outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec3 get_color(int x, int y) {
  ivec2 image_size = imageSize(inputImage);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = coord % image_size;
  return (imageLoad(inputImage, coord).xyz);
}

const float diffusion_coefficient = 0.2;
void main() {
  // vec4 color = get_color(0, 0);
  // vec4 left_color = get_color(-1, 0);
  // vec4 right_color = get_color(1, 0);
  // vec4 bottom_color = get_color(0, -1);
  // vec4 top_color = get_color(0, 1);

  // vec4 sum = left_color + right_color + bottom_color + top_color;
  // color = color + diffusion_coefficient * (sum - 4 * color);

  // ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  // imageStore(outputImage, coord, color);

  //<CentralFiniteDifferencesScale.
  vec3 CScale= vec3(1.0f/2.0f);
  float dt = 0.01f;
  float K = 0.5f;
  float v = 1.f;
  float S=K/dt;
  vec3 fC = get_color(0, 0);
  vec3 fL = get_color(-1, 0);
  vec3 fR = get_color(1, 0);
  vec3 fD = get_color(0, -1);
  vec3 fT = get_color(0, 1);
  // du/dx,du/dy
  vec3 Udx = (fR-fL)*CScale;
  vec3 Udy = (fT-fD)*CScale;
  float Udiv = Udx.x+Udy.y;
  vec2 DdX = vec2(Udx.z,Udy.z);
  //<Solvefordensity.
  fC.z -= dt*dot(vec3(DdX,Udiv),fC.xyz);
  //<Relatedtostability.
  fC.z=clamp(fC.z,0.0f,3.0f);
  //<SolveforVelocity.
  vec2 PdX=S*DdX;
  vec2 Laplacian=(fL.xy + fR.xy + fD.xy + fT.xy)-4.0f*fC.xy;
  vec2 ViscosityForce=v*Laplacian;
  //<Semi−lagrangianadvection.
  // vec2 Was=UV-dt*fC.xy*Step;
  // fC.xy=tex2D(FieldLinearSampler,Was).xy;
  // fC.xy+=dt*(ViscosityForce-PdX);
  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  imageStore(outputImage, coord, vec4(fC, 1.f));
  // fC.xy+=dt*(ViscosityForce-PdX+ExternalForces);
  //<Boundaryconditions.
  // for(inti=0;i<4;++i)
  // {
  //   if(IsBoundary(UV+Step∗Directions[i]))
  //   {
  //     float2SetToZero=(1−abs(Directions[i]));
  //     FC.xy∗=SetToZero;
  //   }
  // }
}
