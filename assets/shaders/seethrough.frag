// Voir à travers le décor (avec liquid.vert, qui donne la position dans le monde) : autour de chaque
// personnage caché derrière cette tuile (arbre, rocher, bloc de mur), la tuile devient transparente
// dans une ellipse aux bords adoucis.
uniform sampler2D texture;
uniform vec2 u_holes[8];		// Centres des ellipses (repère du monde)
uniform vec2 u_radii[8];		// Demi-axes des ellipses
uniform int u_count;
varying vec2 v_world;

void main()
{
    vec4 color = texture2D(texture, gl_TexCoord[0].xy) * gl_Color;
    float keep = 1.0;
    for (int i = 0; i < 8; i++)
    {
        if (i >= u_count)
            break;
        float distance = length((v_world - u_holes[i]) / u_radii[i]);
        keep = min(keep, mix(0.28, 1.0, smoothstep(0.7, 1.0, distance)));
    }
    gl_FragColor = vec4(color.rgb, color.a * keep);
}
