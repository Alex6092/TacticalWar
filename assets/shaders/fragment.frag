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

	if (pixelMask.r > pixelMask.b && pixelMask.r > pixelMask.g && pixelMask.r > 0.20)
	{
		pixel *= color1;
	}
	else if (pixelMask.b > pixelMask.r && pixelMask.b > pixelMask.g && pixelMask.b > 0.20)
	{
		pixel *= color2;

	}else if (pixelMask.g > pixelMask.r && pixelMask.g > pixelMask.b && pixelMask.g > 0.20)
	{
		pixel *= color3;
	}

	
    // et multiplication avec la couleur pour obtenir le pixel final
    gl_FragColor = gl_Color * pixel;
}