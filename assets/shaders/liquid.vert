// Liquides (eau, lave) : transmet la position dans le monde au fragment shader, pour que les
// vagues soient continues d'une case à l'autre.
varying vec2 v_world;

void main()
{
    vec4 world = gl_ModelViewMatrix * gl_Vertex;
    v_world = world.xy;
    gl_Position = gl_ProjectionMatrix * world;
    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
    gl_FrontColor = gl_Color;
}
