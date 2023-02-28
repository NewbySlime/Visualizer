shader_type canvas_item;

// main data option
// Static | RGB | Sound
uniform int color_type = 0;

uniform sampler2D coldata;
uniform int coldatalen;

uniform float slide_offset = 0.0f;
uniform float brightness = 1.0f;

uniform bool use_splits = false;


// static data option
uniform float static_valueposx = 0.0f;

// rgb data option
uniform float rgb_window = 1.f;
uniform bool rgb_inunion = false;

// sound data option
uniform int color_shift;
uniform int brightness_mode;
uniform float min_intensity;
uniform float max_intensity;

uniform float avg_intensity;

uniform sampler2D floatArray;
uniform int channelCount = 1;
uniform int arrayLength;

uniform sampler2D splitsData;
uniform sampler2D splitsDataLed;
uniform int splitsDataLen;

uniform int manyLed;

uniform float test1;
uniform int test2;

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

int fTo255(float f){
	return int(round(f*255.f));
}

int f2Toffff(float f1, float f2){
	return (fTo255(f1) | (fTo255(f2) << 8));
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

float avgv3(vec3 v){
	return (v.x+v.y+v.z)/3.f;
}

// BUG since the top of the bound is the same as the bottom of the next button, it can't be synced
// return rounded uv of x, and the index in float
vec2 getSplitIndex(float posx){
	float newposx = floor(posx*float(manyLed))/float(manyLed);
	for(int i = 0; i < splitsDataLen; i++){
		vec4 _dataidx = texture(splitsDataLed, vec2((float(i)+0.5f)/float(splitsDataLen), 0.5f));
		if(newposx >= _dataidx.r && newposx < _dataidx.g)
			return vec2(
				(newposx-_dataidx.r)/(_dataidx.g-_dataidx.r),
				float(i)
			);
	}
	
	return vec2(0);
}

float getIntensity(float posx, int channel){
	return texture(floatArray, vec2((posx+float(channel))/float(channelCount)), 0.5f).r;
}

void fragment(){
	vec2 _newuv = UV;
	vec4 last_col = vec4(1.f);
	int currchannel = 0;
	int coltype = color_type;
	float intensity;
	
	if(use_splits){
		vec2 _currentidx_split = getSplitIndex(UV.x);
		int _currentidx = int(_currentidx_split.y);
		vec4 _datasplit = texture(splitsData, vec2((float(_currentidx)+0.5)/float(splitsDataLen), 0.5f));
		int _additionalData = int(round(_datasplit.z*float(0xff)));
		bool reversed = (_additionalData & 0x1) == 1;
		currchannel = (_additionalData & 0xff) >> 1;
		
		float _delta = _datasplit.y - _datasplit.x;
		_newuv.x = _delta*_currentidx_split.x;
		if(reversed)
			_newuv.x = _delta-_newuv.x;
		
		_newuv.x += _datasplit.x;
	}
	
	if(color_type == 2){
		if(arrayLength <= 0)
			return;
		
		coltype = color_shift;
		intensity = getIntensity(_newuv.x, currchannel);
		switch(brightness_mode){
			// not going to use, since not modified
			// case 0
			
			// union
			case 1:{
				float deltaDb = avg_intensity-min_intensity;
				float deltaDbParam = (max_intensity-min_intensity)*(1f-_newuv.y);
				if(deltaDb < 0.f)
					return;
				
				if(deltaDbParam > deltaDb)
					return;
				
				last_col = texture(TEXTURE, vec2(_newuv.x, _newuv.y-(1.0f-deltaDb/max_intensity)));
				break;
			}
			
			// individual
			case 2:{
				float deltaDb = intensity-min_intensity;
				float deltaDbParam = (max_intensity-min_intensity)*(1f-_newuv.y);
				
				if(deltaDbParam > deltaDb)
					return;

        float uvy = 0.f;
				if(deltaDb >= 0.f)
					uvy = _newuv.y-(1.0f-deltaDb/max_intensity);
				
				last_col = texture(TEXTURE, vec2(_newuv.x, uvy));
				break;
			}
		}
		
		if(rgb_inunion && color_shift == 2)
			intensity = avg_intensity;
	}
	
	switch(coltype){
		// static color type
		case 0:
			last_col *= vec4(_getcolorbetween(static_valueposx), 1);
			break;
		
		// RGB color type
		case 1:{
			float shiftx = slide_offset;
			if(!rgb_inunion)
				shiftx += (_newuv.x*rgb_window);
				
			shiftx = mod(shiftx, 1);
			last_col *= vec4(_getcolorbetween(shiftx), 1);
			break;
		}
		
		// sound color type
		case 2:{
			last_col *= vec4(_getcolorbetween((intensity-min_intensity)/(max_intensity-min_intensity)), 1);
			break;
		}
	}
	
	COLOR = last_col*texture(TEXTURE, vec2(_newuv.x, _newuv.y*max(max(last_col.x, last_col.y), last_col.z)*brightness));
	//COLOR = vec4(round(test1 * float(0xff)) == 1.f? 1.0f: 0.0f);
}