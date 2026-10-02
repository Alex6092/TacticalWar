// Liquides : eau (vagues, scintillements, écume au bord, reflets) et lave (croûte et lueur).
// Seule la face supérieure de la tuile est animée : les flancs du bloc restent fixes.

uniform sampler2D texture;
uniform sampler2D u_reflection;     // Personnages et décor dessinés à l'envers (même vue que l'écran)
uniform float u_time;
uniform vec2 u_surface;             // Centre de la surface du liquide (repère du monde)
uniform vec2 u_half;                // Demi-largeur et demi-hauteur du losange de la surface
uniform vec4 u_shore;               // 1 si le bord touche une case sans liquide : +x, -x, +y, -y
uniform vec2 u_textureSize;
uniform vec2 u_resolution;          // Taille de la fenêtre (pour lire les reflets)
uniform float u_reflectionStrength; // 0 : pas de reflets
uniform float u_lava;               // 1 : lave

varying vec2 v_world;

void main()
{
    // Coordonnées dans la case, le long des axes de la grille : [-0.5, 0.5] sur la surface.
    vec2 d = v_world - u_surface;
    float gx = 0.5 * (d.x / u_half.x + d.y / u_half.y);
    float gy = 0.5 * (d.y / u_half.y - d.x / u_half.x);
    float surface = 1.0 - smoothstep(0.42, 0.5, max(abs(gx), abs(gy)));

    // Vagues dans le repère du monde.
    vec2 wave = vec2(sin(v_world.y * 0.11 + u_time * 1.7) + 0.5 * sin(v_world.x * 0.05 - u_time * 1.1),
                     cos(v_world.x * 0.07 + u_time * 1.3) + 0.5 * cos((v_world.x + v_world.y) * 0.04 + u_time * 0.9));
    float strength = u_lava > 0.5 ? 0.6 : 1.3;
    vec4 original = texture2D(texture, gl_TexCoord[0].xy);
    vec4 base = texture2D(texture, gl_TexCoord[0].xy + wave * strength * surface / u_textureSize);
    vec3 color = mix(original.rgb, base.rgb, base.a);

    if (u_lava > 0.5)
    {
        // Croûte sombre qui dérive lentement, fissures lumineuses et lueur pulsée.
        float n = sin(v_world.x * 0.045 + u_time * 0.5) * sin(v_world.y * 0.08 - u_time * 0.4)
            + 0.6 * sin((v_world.x - v_world.y) * 0.03 + u_time * 0.25);
        float crust = smoothstep(0.35, 0.9, n);
        float pulse = 0.85 + 0.15 * sin(u_time * 2.2 + (v_world.x + v_world.y) * 0.02);
        vec3 hot = color * 1.35 * pulse + vec3(0.25, 0.12, 0.0) * pulse;
        color = mix(color, mix(hot, color * 0.35, crust * 0.85), surface);
    }
    else
    {
        if (u_reflectionStrength > 0.0)
        {
            vec2 screen = gl_FragCoord.xy / u_resolution + wave * vec2(0.0012, 0.0008);
            vec4 reflection = texture2D(u_reflection, screen);
            color = mix(color, reflection.rgb * vec3(0.7, 0.85, 1.0), reflection.a * u_reflectionStrength * surface);
        }

        float glint = sin(v_world.x * 0.09 + u_time * 2.1) * sin(v_world.y * 0.15 - u_time * 1.7)
            + 0.5 * sin((v_world.x + v_world.y) * 0.05 + u_time);
        color += vec3(1.0) * smoothstep(1.05, 1.4, glint) * 0.3 * surface;

        float foam = max(max(u_shore.x * smoothstep(0.26, 0.46, gx), u_shore.y * smoothstep(0.26, 0.46, -gx)),
                         max(u_shore.z * smoothstep(0.26, 0.46, gy), u_shore.w * smoothstep(0.26, 0.46, -gy)));
        float ripple = 0.55 + 0.45 * sin((gx + gy) * 38.0 - u_time * 2.5 + sin(u_time * 0.7) * 2.0);
        color = mix(color, vec3(0.92, 0.97, 1.0), foam * ripple * 0.75 * surface);
    }

    gl_FragColor = vec4(color, original.a) * gl_Color;
}
