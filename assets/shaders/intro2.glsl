
 
#ifdef GL_ES
precision mediump float;
#endif
 
#extension GL_OES_standard_derivatives : enable
 
uniform float time;
uniform vec2 mouse;
uniform vec2 resolution;
 
 
 
#define Resolution              resolution
#define Time                    time
#define HorizontalAmplitude     0.40
#define VerticleAmplitude       0.20
#define HorizontalSpeed         0.90
#define VerticleSpeed           1.50
#define ParticleMinSize         1.80
#define ParticleMaxSize         1.9
#define ParticleBreathingSpeed      0.30
#define ParticleColorChangeSpeed    0.70
#define ParticleCount           2.0
#define ParticleColor1          vec3(9.0, 5.0, 3.0)
#define ParticleColor2          vec3(1.0, 3.0, 9.0)
#define font_size 20. 
#define font_spacing .05
#define A_ vec2(0.,0.)
#define B_ vec2(1.,0.)
#define C_ vec2(2.,0.)
#define D_ vec2(0.,1.)
#define E_ vec2(1.,1.)
#define F_ vec2(2.,1.)
#define G_ vec2(0.,2.)
#define H_ vec2(1.,2.)
#define I_ vec2(2.,2.)
#define J_ vec2(0.,3.)
#define K_ vec2(1.,3.)
#define L_ vec2(2.,3.)
#define M_ vec2(0.,4.)
#define N_ vec2(1.,4.)
#define O_ vec2(2.,4.)
#define P_ vec2(0.,5.)
#define Q_ vec2(1.,5.)
#define R_ vec2(1.,5.)
#define S_ vec2(0.,6.)
#define T_ vec2(1.,6.)
#define U_ vec2(2.0,6.)
#define A(p) t(G_,I_,p) + t(I_,O_,p) + t(O_,M_, p) + t(M_,J_,p) + t(J_,L_,p)
#define B(p) t(A_,M_,p) + t(M_,O_,p) + t(O_,I_, p) + t(I_,G_,p)
#define C(p) t(I_,G_,p) + t(G_,M_,p) + t(M_,O_,p) 
#define D(p) t(C_,O_,p) + t(O_,M_,p) + t(M_,G_,p) + t(G_,I_,p)
#define E(p) t(O_,M_,p) + t(M_,G_,p) + t(G_,I_,p) + t(I_,L_,p) + t(L_,J_,p)
#define F(p) t(C_,B_,p) + t(B_,N_,p) + t(G_,I_,p)
#define G(p) t(O_,M_,p) + t(M_,G_,p) + t(G_,I_,p) + t(I_,U_,p) + t(U_,S_,p)
#define H(p) t(A_,M_,p) + t(G_,I_,p) + t(I_,O_,p) 
#define I(p) t(E_,E_,p) + t(H_,N_,p) 
#define J(p) t(E_,E_,p) + t(H_,T_,p) + t(T_,S_,p)
#define K(p) t(A_,M_,p) + t(M_,I_,p) + t(K_,O_,p)
#define L(p) t(B_,N_,p)
#define M(p) t(M_,G_,p) + t(G_,I_,p) + t(H_,N_,p) + t(I_,O_,p)
#define N(p) t(M_,G_,p) + t(G_,I_,p) + t(I_,O_,p)
#define O(p) t(G_,I_,p) + t(I_,O_,p) + t(O_,M_, p) + t(M_,G_,p)
#define P(p) t(S_,G_,p) + t(G_,I_,p) + t(I_,O_,p) + t(O_,M_, p)
#define Q(p) t(U_,I_,p) + t(I_,G_,p) + t(G_,M_,p) + t(M_,O_, p)
#define R(p) t(M_,G_,p) + t(G_,I_,p)
#define S(p) t(I_,G_,p) + t(G_,J_,p) + t(J_,L_,p) + t(L_,O_,p) + t(O_,M_,p)
#define T(p) t(B_,N_,p) + t(N_,O_,p) + t(G_,I_,p)
#define U(p) t(G_,M_,p) + t(M_,O_,p) + t(O_,I_,p)
#define V(p) t(G_,J_,p) + t(J_,N_,p) + t(N_,L_,p) + t(L_,I_,p)
#define W(p) t(G_,M_,p) + t(M_,O_,p) + t(N_,H_,p) + t(O_,I_,p)
#define X(p) t(G_,O_,p) + t(I_,M_,p)
#define Y(p) t(G_,M_,p) + t(M_,O_,p) + t(I_,U_,p) + t(U_,S_,p)
#define Z(p) t(G_,I_,p) + t(I_,M_,p) + t(M_,O_,p)
#define g(p) t(A_,G_,p) + t(G_,M_,p) + t(M_,O_,p) + t(C_,F_,p) + t(C_,A_,p)+ t(O_,I_,p)+ t(I_,H_,p)


vec2 caret_origin = vec2(3.0, .7);
vec2 caret;

//-----------------------------------------------------------------------------------
float minimum_distance(vec2 v, vec2 w, vec2 p_)
{	
  	float l2 = (v.x - w.x)*(v.x - w.x) + (v.y - w.y)*(v.y - w.y); 
  	if (l2 == 0.0) {
		return distance(p_, v);  
	}
	
  	float t = dot(p_ - v, w - v) / l2;
  	if(t < 0.0) {
		
		return distance(p_, v);
	} else if (t > 1.0) {
		return distance(p_, w);  
	}
  	vec2 projection = v + t * (w - v);  
	return distance(p_, projection);
}


//-----------------------------------------------------------------------------------
float textColor(vec2 from, vec2 to, vec2 p_)
{
	p_ *= font_size;
	float inkNess = 0., nearLine, corner, strokeWidth = 0.05; 
	nearLine = minimum_distance(from,to,p_); 
	inkNess += smoothstep(0., 6., 1.- 50.*(nearLine - strokeWidth)); 
	inkNess += smoothstep(0., 2.5, 1.- (nearLine  + 5. * strokeWidth));
	return inkNess;
}

//-----------------------------------------------------------------------------------

vec2 grid(vec2 letterspace) 
{
	return ( vec2( (letterspace.x / 2.) * .65 , 1.0-((letterspace.y / 2.) * 1.5) ));
}

//-----------------------------------------------------------------------------------
float count = 0.0;
float t(vec2 from, vec2 to, vec2 p_) 
{
	count++;
	if (count > time*20.0) return 0.0;
	return textColor(grid(from), grid(to), p_);
}

//-----------------------------------------------------------------------------------
vec2 r()
{
	vec2 pos = gl_FragCoord.xy/resolution.xy;
	pos.y -= caret.y;
   	pos.x -= font_spacing*caret.x;
	return pos;
}

//-----------------------------------------------------------------------------------




float hash( float x )
{
    return fract( sin( x ) * 43758.5453 );
}
 
float noise( vec2 uv )  
{
    vec3 x = vec3( uv.xy, 0.0 );
    
    vec3 p = floor( x );
    vec3 f = fract( x );
    
    f = f*f*(3.0 - 2.0*f);
    
    float offset = 7.0;
    
    float n = dot( p, vec3(1.0, offset, offset*2.0) );
    
    return mix( mix(    mix( hash( n + 0.0 ),       hash( n + 1.0 ), f.x ),
                        mix( hash( n + offset),     hash( n + offset+1.0), f.x ), f.y ),
                mix(    mix( hash( n + offset*2.0), hash( n + offset*2.0+1.0), f.x),
                        mix( hash( n + offset*3.0), hash( n + offset*3.0+1.0), f.x), f.y), f.z);
}
 
float snoise( vec2 uv )
{
    return noise( uv ) * 2.0 - 1.0;
}
 
 
float perlinNoise( vec2 uv )
{   
    float n =   noise( uv * 1.0 )   * 128.0 +
                noise( uv * 2.0 )   * 64.0 +
                noise( uv * 4.0 )   * 32.0 +
                noise( uv * 8.0 )   * 16.0 +
                noise( uv * 16.0 )  * 8.0 +
                noise( uv * 32.0 )  * 4.0 +
                noise( uv * 64.0 )  * 2.0 +
                noise( uv * 128.0 ) * 1.0;
    
    float noiseVal = n / ( 1.0 + 2.0 + 4.0 + 8.0 + 16.0 + 32.0 + 64.0 + 128.0 );
	
    noiseVal = abs(noiseVal * 2.0 - 1.0);
    
    return  noiseVal;
	
}
 
float fBm( vec2 uv, float lacunarity, float gain )
{
    float sum = 0.0;
    float amp = 7.0;
    
    for( int i = 0; i < 2; ++i )
    {
        sum += ( perlinNoise( uv ) ) * amp;
        amp *= gain;
        uv *= lacunarity;
    }
    
    return sum;
}
 
vec3 particles( vec2 pos )
{
    
    vec3 c = vec3( 0, 0, 0 );
    
    float noiseFactor = fBm( pos, 0.01, 0.1);
    
//------------------------------------------------------------------------------------------------------------- 
   	
    for( float i = 1.0; i < ParticleCount+1.0; ++i )
    {
        float cs = cos( time * HorizontalSpeed * (i/ParticleCount) + noiseFactor ) * HorizontalAmplitude;
        float ss = sin( time * VerticleSpeed   * (i/ParticleCount) + noiseFactor ) * VerticleAmplitude;
        vec2 origin = vec2( cs , ss );
        
        float t = sin( time * ParticleBreathingSpeed * i ) * 0.5 + 0.5;
        float particleSize = mix( ParticleMinSize, ParticleMaxSize, t );
        float d = clamp( sin( length( pos - origin )  + particleSize ), 0.0, particleSize);
        
        float t2 = sin( time * ParticleColorChangeSpeed * i ) * 0.5 + 0.5;
        vec3 color = mix( ParticleColor1, ParticleColor2, t2 );
        c += color * pow( d, 10.0 );
    }
   
//--------------------------------------------------------------------------------------------------------------    
    return c;
}
 
 
float line( vec2 a, vec2 b, vec2 p )
{
    vec2 aTob = b - a;
    vec2 aTop = p - a;
    
    float t = dot( aTop, aTob ) / dot( aTob, aTob);
    
    t = clamp( t, 0.0, 1.0);
    
    float d = length( p - (a + aTob * t) );
    d = 0.85 / d;
    
    return clamp( d, 0.0, 1.0 );
}
 
 

//-----------------------------------------------------------------------------------
void add()
{
	caret.x += 1.0;
	
}

//-----------------------------------------------------------------------------------
void space()
{
	caret.x += 1.5;
}

//-----------------------------------------------------------------------------------
void newline()
{
	caret.x = caret_origin.x;
	caret.y -= .28;
}

//-----------------------------------------------------------------------------------




void main( void ) {
    
	
	float d_ = 0.;
	vec3 col = vec3((0.1,0.25,0.02));
	caret_origin = vec2(3.85, .85);
	caret = caret_origin;

	
	d_ += T(r()); add(); d_ += A(r()); add(); d_ += C(r()); add(); d_ += T(r()); add(); d_ += I(r()); add();
	d_ += C(r()); add(); d_ += A(r()); add(); 
	d_ += L(r()); add(); space(); d_ += W(r()); add(); d_ += A(r()); add(); d_ += R(r());
	//newline();
	//newline();
	//d_ += C(r()); add(); d_ += O(r()); add(); d_ += M(r()); add(); d_ += E(r()); space();
	//d_ += T(r()); add(); d_ += O(r()); space();add(); d_ += J(r()); add(); d_ += E(r()); add(); d_ += S(r()); add(); d_ += U(r()); add(); d_ += S(r()); space();
	
	
    float aspectRatio = resolution.x / resolution.y;
    
    vec2 uv = ( gl_FragCoord.xy / resolution.xy );
    
    vec2 signedUV = uv * 2.0 - 1.0;
    signedUV.x *= aspectRatio;
    signedUV.y -= 0.28;
    
	
    float scale = 100.0;
    const float v = 80.0;
    vec3 finalColor = vec3( 0.0 );
    
    
    finalColor = (particles( sin( abs(signedUV) ) ) * length(signedUV)) * 0.20;
   
    float t = line( vec2(0.0, v * 0.175 - 22.), vec2(0.0, -v * 0.8 + 0.), signedUV * scale );
    finalColor += vec3( 8.0 * t, 4.0 * t, 1.50 * t) * 0.3;
    
    t = line( vec2(-v * 0.3, -v*0.1 + 0.), vec2(v * 0.3, -v*0.1 + 0.), signedUV * scale );
    finalColor += vec3( 13.0 * t, 4.0 * t, 1.50 * t) * 0.3;
    
    
    col += vec3(d_*.0, d_, d_*1.75);
    float t_ = sin(time/2.);

	
    gl_FragColor = vec4( col+finalColor/t_, 1.0 );
   	
	
}








