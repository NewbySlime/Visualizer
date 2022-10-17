shader_type canvas_item;

uniform sampler2D coldata;
uniform int coldatalen;

vec3 _getcolorbetween(float range){
	vec4 col1, col2;
	int i = 0;
	while(i < coldatalen){
		vec4 _data = texture(coldata, vec2((float(i)+0.5)/float(coldatalen), 0.5f));
		if(range < _data.a)
			break;
		
		i++;
	}
	
	
	col1 = texture(coldata, vec2((float(i-1)+0.5)/float(coldatalen), 0.5f));
	col2 = texture(coldata, vec2((float(i)+0.5)/float(coldatalen), 0.5f));
	
	if(i == 0){
		col1 = col2;
		col2 = texture(coldata, vec2(1.f, 0.5f));
		col2.a -= 1.0f;
	}
	 if(i == coldatalen){
		col1 = col2;
		col2 = texture(coldata, vec2(0.f, 0.5f));
		col2.a += 1.0f;
	}
	
	return mix(col1.xyz, col2.xyz, (range-col1.a)/(col2.a-col1.a));
}

void fragment(){
	COLOR = vec4(_getcolorbetween(UV.x), 1);
}