#ifdef GL_ES
precision mediump float;
#endif

#extension GL_OES_standard_derivatives : enable

uniform float time;
uniform vec2 mouse;
uniform vec2 resolution;

void main( void ) {
	vec2 p = (gl_FragCoord.xy * 2.0 - resolution) / min(resolution.x, resolution.y);
	vec3 destColor = vec3(0.9, 0.6, 1.0);
	float f = 0.0;
	float radius = (sin(time)+1.0)*0.07;
	for(float i = 0.0; i < 10.0; i++)
	{
		float theta = 3.0*sin(time) + i * 0.628318;
		float s = sin(theta) * 0.3;
		float c = cos(theta) * 1.0;
		float equ = abs(length((p - vec2(c, s))/vec2(c, 1.0)) - radius);
		f += 0.0068 / equ*3.0;
	}
	gl_FragColor = vec4(vec3(destColor * f), 1.0);
}