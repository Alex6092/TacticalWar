uniform sampler2D texture;
uniform sampler2D mask;

uniform vec4 color1;
uniform vec4 color2;
uniform vec4 color3;

void main()
{
    // récupère le pixel dans la texture
    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
	vec4 pixelMask = texture2D(mask, gl_TexCoord[0].xy);
	
	vec4 result;
	float lvl = 0.299 * pixel.r + 0.587 * pixel.g + 0.114 * pixel.b;
	result.r = lvl;
	result.g = lvl;
	result.b = lvl;
	
	
    // et multiplication avec la couleur pour obtenir le pixel final
    gl_FragColor = gl_Color * result;
}