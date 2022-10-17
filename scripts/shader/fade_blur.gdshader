shader_type canvas_item;

uniform float blur;
uniform float blur_mult;

void fragment(){
	COLOR = texture(SCREEN_TEXTURE, SCREEN_UV, blur*blur_mult) - (texture(TEXTURE, SCREEN_UV) * blur);
}