shader_type canvas_item;

uniform sampler2D floatArray;
uniform int arrayLength;

uniform float dbMin;
uniform float dbMax;

const float testarr[] = {20f, 50f, 40f, 80f};
const int testarrlen = 4;

//uniform vec4 colorMax: hint_color;
//uniform vec4 colorMin: hint_color;

int fTo255(float f){
	return int(round(f*255.f));
}

int vec4ToInt(vec4 data){
	return fTo255(data.a) | (fTo255(data.b) << 8) | (fTo255(data.g) << 16) | (fTo255(data.r) << 24);
}

float vec4ToFloat(vec4 data){
	int datai = vec4ToInt(data);
	int exponent = (datai >> 23) & 0b011111111;
	const float exponentDivide = float(0x800000);
	if(exponent == 0)
		return pow(-1.f, float(datai >> 31)) * pow(2.f, -126.f) * (float(datai & 0x7fffff)/exponentDivide);
	else
		return pow(-1.f, float(datai >> 31)) * pow(2.f, float(exponent-127)) * (1.f + float(datai & 0x7fffff)/exponentDivide);
}

void fragment(){
	if(arrayLength <= 0)
		return;
		
	//vec4 floatData = texture(floatArray, vec2(UV.x, 0.5));
	//float currentFloat = vec4ToFloat(floatData);
	
	//float currentFloat = testarr[int(floor(UV.x*float(testarrlen)))];
	
	float currentFloat = texture(floatArray, vec2(UV.x, 0.5)).r;
	
	float deltaDb = (dbMax-dbMin)*currentFloat;
	float deltaDbParam = (dbMax-dbMin)*(1f-UV.y);
	if(deltaDb < 0.f)
		return;
	
	if(deltaDbParam > deltaDb)
		return;
	
	COLOR = texture(TEXTURE, UV);
}