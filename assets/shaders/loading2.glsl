// PS2 Opening
// Please I want you fork enhance the PS2 feel
precision mediump float;
uniform float time; // time
uniform vec2  resolution; // resolution

void main(void){
    vec2 p = (gl_FragCoord.xy * 2.0 - resolution) / min(resolution.x, resolution.y);
    vec3 destColor = vec3(0.3, 0.6, 1.0);
    float f = 0.0;
	
    for(float i = 0.0; i < 20.0; i++){
	float s = sin(0.7 * time + (i * 0.5) * time) * 0.3;
	float c = cos(0.2 * time + (i * 0.5) * time) * 0.3 ;
        f += 0.01 / abs(length(p*0.5 + vec2(c, s)));
    }
	
    gl_FragColor = vec4(vec3(destColor * f), 1.0);
}