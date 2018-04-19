
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined(__APPLE__)
#include <GLUT/GLUT.h>
#include <OpenGL/gl3.h>
#include <OpenGL/glu.h>
#else
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#endif
#include <GL/glew.h>		 
#include <GL/freeglut.h>	
#endif

#include <string>
#include <vector>
#include <fstream>
#include <algorithm> 
const unsigned int windowWidth = 512, windowHeight = 512;

int majorVersion = 3, minorVersion = 0;

bool keyboardState[256];

void getErrorInfo(unsigned int handle) 
{
	int logLen;
	glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
	if (logLen > 0) 
	{
		char * log = new char[logLen];
		int written;
		glGetShaderInfoLog(handle, logLen, &written, log);
		printf("Shader log:\n%s", log);
		delete log;
	}
}

void checkShader(unsigned int shader, char * message) 
{
	int OK;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &OK);
	if (!OK) 
	{
		printf("%s!\n", message);
		getErrorInfo(shader);
	}
}

void checkLinking(unsigned int program) 
{
	int OK;
	glGetProgramiv(program, GL_LINK_STATUS, &OK);
	if (!OK) 
	{
		printf("Failed to link shader program!\n");
		getErrorInfo(program);
	}
}

// row-major matrix 4x4
struct mat4 
{
	float m[4][4];
public:
	mat4() {}
	mat4(float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33) 
	{
		m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
		m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
		m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
		m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
	}

	mat4 operator*(const mat4& right) 
	{
		mat4 result;
		for (int i = 0; i < 4; i++) 
		{
			for (int j = 0; j < 4; j++) 
			{
				result.m[i][j] = 0;
				for (int k = 0; k < 4; k++) result.m[i][j] += m[i][k] * right.m[k][j];
			}
		}
		return result;
	}
	operator float*() { return &m[0][0]; }
};


// 3D point in homogeneous coordinates
struct vec4 
{
	float v[4];
    float x, y, z, w;

	vec4(float x = 0, float y = 0, float z = 0, float w = 1)  : x(x), y(y), z(z), w(w)
	{
		v[0] = x; v[1] = y; v[2] = z; v[3] = w;
	}

	vec4 operator*(const mat4& mat) 
	{
		vec4 result;
		for (int j = 0; j < 4; j++) 
		{
			result.v[j] = 0;
			for (int i = 0; i < 4; i++) result.v[j] += v[i] * mat.m[i][j];
		}
		return result;
	}

	vec4 operator+(const vec4& vec) 
	{
		vec4 result(x + vec.x, y + vec.y, z + vec.z, w + vec.w);
		return result;
	}
};

// 2D point in Cartesian coordinates
struct vec2 
{
	float x, y;

	vec2(float x = 0.0, float y = 0.0) : x(x), y(y) {}

	vec2 operator+(const vec2& v) 
	{
		return vec2(x + v.x, y + v.y);
	}

	vec2 operator*(float s) 
	{
		return vec2(x * s, y * s);
	}

};

// 3D point in Cartesian coordinates
struct vec3 
{
	float x, y, z;

	vec3(float x = 0.0, float y = 0.0, float z = 0.0) : x(x), y(y), z(z) {}

	static vec3 random() { return vec3(((float)rand() / RAND_MAX) * 2 - 1, ((float)rand() / RAND_MAX) * 2 - 1, ((float)rand() / RAND_MAX) * 2 - 1); }

	vec3 operator+(const vec3& v) { return vec3(x + v.x, y + v.y, z + v.z); }

	vec3 operator-(const vec3& v) { return vec3(x - v.x, y - v.y, z - v.z); }

	vec3 operator*(float s) { return vec3(x * s, y * s, z * s); }

	vec3 operator/(float s) { return vec3(x / s, y / s, z / s); }

	float length() { return sqrt(x * x + y * y + z * z); }

	vec3 normalize() { return *this / length(); }

	void print() { printf("%f \t %f \t %f \n", x, y, z); }
};

vec3 cross(const vec3& a, const vec3& b)
{
	return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x );
}

float dot(vec3& a, vec3& b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}


class Geometry
{
protected:
	unsigned int vao;

public:
	Geometry()
	{
		glGenVertexArrays(1, &vao);					
	}

	virtual void Draw() = 0;
};


class   PolygonalMesh : public Geometry
{
	struct  Face
	{
		int       positionIndices[4];
		int       normalIndices[4];
		int       texcoordIndices[4];
		bool      isQuad;
	};

	std::vector<std::string*> rows;
	std::vector<vec3*> positions;
	std::vector<std::vector<Face*> > submeshFaces;
	std::vector<vec3*> normals;
	std::vector<vec2*> texcoords;

	int nTriangles;

public:
	PolygonalMesh(const char *filename);
	~PolygonalMesh();

	void Draw();
};

class TexturedQuad: public Geometry
{

public:
    TexturedQuad() {
        unsigned int vbo[3];

        glBindVertexArray(vao);

        glGenBuffers(3, &vbo[0]);

        // vertex pos
        glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
        static float vertexCoords[] = {0.0, 0.0, 0.0, 1.0,
                                       -1.0, 0.0, 1.0, 0.0,
                                       -1.0, 0.0, -1.0, 0.0,
                                       1.0, 0.0, -1.0, 0.0,
                                      1.0, 0.0, 1.0, 0.0,
                                       -1.0, 0.0, 1.0, 0.0};
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);

        //texture coords
        glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
        static float texCoords[] = {0.0, 1.0,
                                     0.0, 0.0,
                                     1.0, 0.0,
                                     1.0, 1.0};
        glBufferData(GL_ARRAY_BUFFER, sizeof(texCoords), texCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        
        // normals
        glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
        static float normCoords[] = {0.0, 1.0, 0.0,
                                     0.0, 1.0, 0.0, 
                                     0.0, 1.0, 0.0,
                                     0.0, 1.0, 0.0,
                                     0.0, 1.0, 0.0,
                                     0.0, 1.0, 0.0};
        glBufferData(GL_ARRAY_BUFFER, sizeof(normCoords), normCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    }

    void Draw() {
        glEnable(GL_DEPTH_TEST);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 6);
        glDisable(GL_DEPTH_TEST);
    }

};



PolygonalMesh::PolygonalMesh(const char *filename)
{
	std::fstream file(filename); 
	if(!file.is_open())       
	{
		return;
	}

	char buffer[256];
	while(!file.eof())
	{
		file.getline(buffer,256);
		rows.push_back(new std::string(buffer));
	}

	submeshFaces.push_back(std::vector<Face*>());
	std::vector<Face*>* faces = &submeshFaces.at(submeshFaces.size()-1);

	for(int i = 0; i < rows.size(); i++)
	{
		if(rows[i]->empty() || (*rows[i])[0] == '#') 
			continue;      
		else if((*rows[i])[0] == 'v' && (*rows[i])[1] == ' ')
		{
			float tmpx,tmpy,tmpz;
			sscanf(rows[i]->c_str(), "v %f %f %f" ,&tmpx,&tmpy,&tmpz);      
			positions.push_back(new vec3(tmpx,tmpy,tmpz));  
		}
		else if((*rows[i])[0] == 'v' && (*rows[i])[1] == 'n')    
		{
			float tmpx,tmpy,tmpz;   
			sscanf(rows[i]->c_str(), "vn %f %f %f" ,&tmpx,&tmpy,&tmpz);
			normals.push_back(new vec3(tmpx,tmpy,tmpz));     
		}
		else if((*rows[i])[0] == 'v' && (*rows[i])[1] == 't')
		{
			float tmpx,tmpy;
			sscanf(rows[i]->c_str(), "vt %f %f" ,&tmpx,&tmpy);
			texcoords.push_back(new vec2(tmpx,tmpy));     
		}
		else if((*rows[i])[0] == 'f')  
		{
			if(count(rows[i]->begin(),rows[i]->end(), ' ') == 3)
			{
				Face* f = new Face();
				f->isQuad = false;
				sscanf(rows[i]->c_str(), "f %d/%d/%d %d/%d/%d %d/%d/%d",
					&f->positionIndices[0], &f->texcoordIndices[0], &f->normalIndices[0],
					&f->positionIndices[1], &f->texcoordIndices[1], &f->normalIndices[1],
					&f->positionIndices[2], &f->texcoordIndices[2], &f->normalIndices[2]);
				faces->push_back(f);
			}
			else
			{
				Face* f = new Face();
				f->isQuad = true;
				sscanf(rows[i]->c_str(), "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d", 
					&f->positionIndices[0], &f->texcoordIndices[0], &f->normalIndices[0],
					&f->positionIndices[1], &f->texcoordIndices[1], &f->normalIndices[1],
					&f->positionIndices[2], &f->texcoordIndices[2], &f->normalIndices[2],
					&f->positionIndices[3], &f->texcoordIndices[3], &f->normalIndices[3]);
				faces->push_back(f);   
			}
		}
		else if((*rows[i])[0] == 'g')
		{
			if(faces->size() > 0)
			{
				submeshFaces.push_back(std::vector<Face*>());
				faces = &submeshFaces.at(submeshFaces.size()-1);
			}
		}
	}
	
	int numberOfTriangles = 0;
	for(int iSubmesh=0; iSubmesh<submeshFaces.size(); iSubmesh++)
	{
		std::vector<Face*>& faces = submeshFaces.at(iSubmesh);

		for(int i=0;i<faces.size();i++)
		{
			if(faces[i]->isQuad) numberOfTriangles += 2;
			else numberOfTriangles += 1;
		}
	}

	nTriangles = numberOfTriangles;
	
	float *vertexCoords = new float[numberOfTriangles * 9];
	float *vertexTexCoords = new float[numberOfTriangles * 6];
	float *vertexNormalCoords = new float[numberOfTriangles * 9];


	int triangleIndex = 0;
	for(int iSubmesh=0; iSubmesh<submeshFaces.size(); iSubmesh++)
	{
		std::vector<Face*>& faces = submeshFaces.at(iSubmesh);

		for(int i=0;i<faces.size();i++)
		{
			if(faces[i]->isQuad) 
			{
				vertexTexCoords[triangleIndex * 6] =     texcoords[faces[i]->texcoordIndices[0]-1]->x;
				vertexTexCoords[triangleIndex * 6 + 1] = 1-texcoords[faces[i]->texcoordIndices[0]-1]->y;
				
				vertexTexCoords[triangleIndex * 6 + 2] = texcoords[faces[i]->texcoordIndices[1]-1]->x;
				vertexTexCoords[triangleIndex * 6 + 3] = 1-texcoords[faces[i]->texcoordIndices[1]-1]->y;

				vertexTexCoords[triangleIndex * 6 + 4] = texcoords[faces[i]->texcoordIndices[2]-1]->x;
				vertexTexCoords[triangleIndex * 6 + 5] = 1-texcoords[faces[i]->texcoordIndices[2]-1]->y;


				vertexCoords[triangleIndex * 9] =     positions[faces[i]->positionIndices[0]-1]->x;
				vertexCoords[triangleIndex * 9 + 1] = positions[faces[i]->positionIndices[0]-1]->y;
				vertexCoords[triangleIndex * 9 + 2] = positions[faces[i]->positionIndices[0]-1]->z;

				vertexCoords[triangleIndex * 9 + 3] = positions[faces[i]->positionIndices[1]-1]->x;
				vertexCoords[triangleIndex * 9 + 4] = positions[faces[i]->positionIndices[1]-1]->y;
				vertexCoords[triangleIndex * 9 + 5] = positions[faces[i]->positionIndices[1]-1]->z;

				vertexCoords[triangleIndex * 9 + 6] = positions[faces[i]->positionIndices[2]-1]->x;
				vertexCoords[triangleIndex * 9 + 7] = positions[faces[i]->positionIndices[2]-1]->y;
				vertexCoords[triangleIndex * 9 + 8] = positions[faces[i]->positionIndices[2]-1]->z;


				vertexNormalCoords[triangleIndex * 9] =     normals[faces[i]->normalIndices[0]-1]->x;    
				vertexNormalCoords[triangleIndex * 9 + 1] = normals[faces[i]->normalIndices[0]-1]->y;
				vertexNormalCoords[triangleIndex * 9 + 2] = normals[faces[i]->normalIndices[0]-1]->z;

				vertexNormalCoords[triangleIndex * 9 + 3] = normals[faces[i]->normalIndices[1]-1]->x;
				vertexNormalCoords[triangleIndex * 9 + 4] = normals[faces[i]->normalIndices[1]-1]->y;
				vertexNormalCoords[triangleIndex * 9 + 5] = normals[faces[i]->normalIndices[1]-1]->z;

				vertexNormalCoords[triangleIndex * 9 + 6] = normals[faces[i]->normalIndices[2]-1]->x;
				vertexNormalCoords[triangleIndex * 9 + 7] = normals[faces[i]->normalIndices[2]-1]->y;
				vertexNormalCoords[triangleIndex * 9 + 8] = normals[faces[i]->normalIndices[2]-1]->z;
								
				triangleIndex++;


				vertexTexCoords[triangleIndex * 6] =     texcoords[faces[i]->texcoordIndices[1]-1]->x;
				vertexTexCoords[triangleIndex * 6 + 1] = 1-texcoords[faces[i]->texcoordIndices[1]-1]->y;
				
				vertexTexCoords[triangleIndex * 6 + 2] = texcoords[faces[i]->texcoordIndices[2]-1]->x;
				vertexTexCoords[triangleIndex * 6 + 3] = 1-texcoords[faces[i]->texcoordIndices[2]-1]->y;

				vertexTexCoords[triangleIndex * 6 + 4] = texcoords[faces[i]->texcoordIndices[3]-1]->x;
				vertexTexCoords[triangleIndex * 6 + 5] = 1-texcoords[faces[i]->texcoordIndices[3]-1]->y;


				vertexCoords[triangleIndex * 9] =     positions[faces[i]->positionIndices[1]-1]->x;
				vertexCoords[triangleIndex * 9 + 1] = positions[faces[i]->positionIndices[1]-1]->y;
				vertexCoords[triangleIndex * 9 + 2] = positions[faces[i]->positionIndices[1]-1]->z;

				vertexCoords[triangleIndex * 9 + 3] = positions[faces[i]->positionIndices[2]-1]->x;
				vertexCoords[triangleIndex * 9 + 4] = positions[faces[i]->positionIndices[2]-1]->y;
				vertexCoords[triangleIndex * 9 + 5] = positions[faces[i]->positionIndices[2]-1]->z;

				vertexCoords[triangleIndex * 9 + 6] = positions[faces[i]->positionIndices[3]-1]->x;
				vertexCoords[triangleIndex * 9 + 7] = positions[faces[i]->positionIndices[3]-1]->y;
				vertexCoords[triangleIndex * 9 + 8] = positions[faces[i]->positionIndices[3]-1]->z;


				vertexNormalCoords[triangleIndex * 9] =     normals[faces[i]->normalIndices[1]-1]->x;
				vertexNormalCoords[triangleIndex * 9 + 1] = normals[faces[i]->normalIndices[1]-1]->y;
				vertexNormalCoords[triangleIndex * 9 + 2] = normals[faces[i]->normalIndices[1]-1]->z;

				vertexNormalCoords[triangleIndex * 9 + 3] = normals[faces[i]->normalIndices[2]-1]->x;
				vertexNormalCoords[triangleIndex * 9 + 4] = normals[faces[i]->normalIndices[2]-1]->y;
				vertexNormalCoords[triangleIndex * 9 + 5] = normals[faces[i]->normalIndices[2]-1]->z;

				vertexNormalCoords[triangleIndex * 9 + 6] = normals[faces[i]->normalIndices[3]-1]->x;
				vertexNormalCoords[triangleIndex * 9 + 7] = normals[faces[i]->normalIndices[3]-1]->y;
				vertexNormalCoords[triangleIndex * 9 + 8] = normals[faces[i]->normalIndices[3]-1]->z;

				triangleIndex++;
			}
			else 
			{
				vertexTexCoords[triangleIndex * 6] =     texcoords[faces[i]->texcoordIndices[0]-1]->x;
				vertexTexCoords[triangleIndex * 6 + 1] = 1-texcoords[faces[i]->texcoordIndices[0]-1]->y;
				
				vertexTexCoords[triangleIndex * 6 + 2] = texcoords[faces[i]->texcoordIndices[1]-1]->x;
				vertexTexCoords[triangleIndex * 6 + 3] = 1-texcoords[faces[i]->texcoordIndices[1]-1]->y;

				vertexTexCoords[triangleIndex * 6 + 4] = texcoords[faces[i]->texcoordIndices[2]-1]->x;
				vertexTexCoords[triangleIndex * 6 + 5] = 1-texcoords[faces[i]->texcoordIndices[2]-1]->y;

				vertexCoords[triangleIndex * 9] =     positions[faces[i]->positionIndices[0]-1]->x;
				vertexCoords[triangleIndex * 9 + 1] = positions[faces[i]->positionIndices[0]-1]->y;
				vertexCoords[triangleIndex * 9 + 2] = positions[faces[i]->positionIndices[0]-1]->z;

				vertexCoords[triangleIndex * 9 + 3] = positions[faces[i]->positionIndices[1]-1]->x;
				vertexCoords[triangleIndex * 9 + 4] = positions[faces[i]->positionIndices[1]-1]->y;
				vertexCoords[triangleIndex * 9 + 5] = positions[faces[i]->positionIndices[1]-1]->z;

				vertexCoords[triangleIndex * 9 + 6] = positions[faces[i]->positionIndices[2]-1]->x;
				vertexCoords[triangleIndex * 9 + 7] = positions[faces[i]->positionIndices[2]-1]->y;
				vertexCoords[triangleIndex * 9 + 8] = positions[faces[i]->positionIndices[2]-1]->z;


				vertexNormalCoords[triangleIndex * 9] =     normals[faces[i]->normalIndices[0]-1]->x;
				vertexNormalCoords[triangleIndex * 9 + 1] = normals[faces[i]->normalIndices[0]-1]->y;
				vertexNormalCoords[triangleIndex * 9 + 2] = normals[faces[i]->normalIndices[0]-1]->z;

				vertexNormalCoords[triangleIndex * 9 + 3] = normals[faces[i]->normalIndices[1]-1]->x;
				vertexNormalCoords[triangleIndex * 9 + 4] = normals[faces[i]->normalIndices[1]-1]->y;
				vertexNormalCoords[triangleIndex * 9 + 5] = normals[faces[i]->normalIndices[1]-1]->z;

				vertexNormalCoords[triangleIndex * 9 + 6] = normals[faces[i]->normalIndices[2]-1]->x;
				vertexNormalCoords[triangleIndex * 9 + 7] = normals[faces[i]->normalIndices[2]-1]->y;
				vertexNormalCoords[triangleIndex * 9 + 8] = normals[faces[i]->normalIndices[2]-1]->z;

				triangleIndex++;
			}
		}
	}

	glBindVertexArray(vao);		

	unsigned int vbo[3];		
	glGenBuffers(3, &vbo[0]);	

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]); 
	glBufferData(GL_ARRAY_BUFFER, nTriangles * 9 * sizeof(float), vertexCoords, GL_STATIC_DRAW);	    
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);     

	glBindBuffer(GL_ARRAY_BUFFER, vbo[1]); 
	glBufferData(GL_ARRAY_BUFFER, nTriangles * 6 * sizeof(float), vertexTexCoords, GL_STATIC_DRAW);	
	glEnableVertexAttribArray(1);  
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL); 
	
	glBindBuffer(GL_ARRAY_BUFFER, vbo[2]); 
	glBufferData(GL_ARRAY_BUFFER, nTriangles * 9 * sizeof(float), vertexNormalCoords, GL_STATIC_DRAW);	    
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, NULL);     
	
	delete vertexCoords;
	delete vertexTexCoords;
	delete vertexNormalCoords;
}


void PolygonalMesh::Draw()
{
	glEnable(GL_DEPTH_TEST);
	glBindVertexArray(vao); 
	glDrawArrays(GL_TRIANGLES, 0, nTriangles * 3);	
	glDisable(GL_DEPTH_TEST);
}


PolygonalMesh::~PolygonalMesh()
{
	for(unsigned int i = 0; i < rows.size(); i++) delete rows[i];   
	for(unsigned int i = 0; i < positions.size(); i++) delete positions[i];
	for(unsigned int i = 0; i < submeshFaces.size(); i++)
		for(unsigned int j = 0; j < submeshFaces.at(i).size(); j++)
			delete submeshFaces.at(i).at(j); 
	for(unsigned int i = 0; i < normals.size(); i++) delete normals[i];
	for(unsigned int i = 0; i < texcoords.size(); i++) delete texcoords[i];
}



class Shader
{
protected:
	unsigned int shaderProgram;

public:
	Shader()
	{
		shaderProgram = 0;
	}

	~Shader()
	{
		if(shaderProgram) glDeleteProgram(shaderProgram);
	}

	void Run()
	{
		if(shaderProgram) glUseProgram(shaderProgram);
	}

	virtual void UploadInvM(mat4& InVM) { }

	virtual void UploadMVP(mat4& MVP) { }

    virtual void UploadVP(mat4& VP) { }
    
    virtual void UploadM(mat4& M) { }

	virtual void UploadColor(vec4& color) { }

	virtual void UploadSamplerID() { }

    virtual void UploadMaterialAttributes(vec3& ka, vec3& kd, vec3& ks, float shininess) { }

    virtual void UploadLightAttributes(vec3& La, vec3& Le, vec4& worldLightPosition) {}

    virtual void UploadEyePosition(vec3& wEye) {}
};



class MeshShader : public Shader
{
public:
	MeshShader()
	{
        const char *vertexSource = "\n\
            #version 130 \n\
            precision highp float; \n\
            in vec3 vertexPosition; \n\
            in vec2 vertexTexCoord; \n\
            in vec3 vertexNormal; \n\
            uniform mat4 M, InvM, MVP; \n\
            uniform vec3 worldEyePosition; \n\
            uniform vec4 worldLightPosition; \n\
            out vec2 texCoord; \n\
            out vec3 worldNormal; \n\
            out vec3 worldView; \n\
            out vec3 worldLight; \n\
            \n\
            void main() { \n\
            texCoord = vertexTexCoord; \n\
            vec4 worldPosition = vec4(vertexPosition, 1) * M; \n\
            worldLight  = worldLightPosition.xyz * worldPosition.w - worldPosition.xyz * worldLightPosition.w; \n\
            worldView = worldEyePosition - worldPosition.xyz; \n\
            worldNormal = (InvM * vec4(vertexNormal, 0.0)).xyz; \n\
            gl_Position = vec4(vertexPosition, 1) * MVP; \n\
            } \n\
            ";

		const char *fragmentSource = "\n\
            #version 130 \n\
            precision highp float; \n\
            uniform sampler2D samplerUnit; \n\
            uniform vec3 La, Le; \n\
            uniform vec3 ka, kd, ks; \n\
            uniform float shininess; \n\
            in vec2 texCoord; \n\
            in vec3 worldNormal; \n\
            in vec3 worldView; \n\
            in vec3 worldLight; \n\
            out vec4 fragmentColor; \n\
            \n\
            void main() { \n\
            vec3 N = normalize(worldNormal); \n\
            vec3 V = normalize(worldView); \n\
            vec3 L = normalize(worldLight); \n\
            vec3 H = normalize(V + L); \n\
            vec3 texel = texture(samplerUnit, texCoord).xyz; \n\
            vec3 color = \n\
            La * ka + \n\
            Le * kd * texel * max(0.0, dot(L, N)) + \n\
            Le * ks * pow(max(0.0, dot(H, N)), shininess); \n\
            fragmentColor = vec4(color, 1); \n\
            } \n\
        ";


		unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
		if (!vertexShader) { printf("Error in vertex shader creation\n"); exit(1); }

		glShaderSource(vertexShader, 1, &vertexSource, NULL);
		glCompileShader(vertexShader);
		checkShader(vertexShader, "Vertex shader error");

		unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		if (!fragmentShader) { printf("Error in fragment shader creation\n"); exit(1); }

		glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
		glCompileShader(fragmentShader);
		checkShader(fragmentShader, "Fragment shader error");

		shaderProgram = glCreateProgram();
		if (!shaderProgram) { printf("Error in shader program creation\n"); exit(1); }

		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);

		glBindAttribLocation(shaderProgram, 0, "vertexPosition");
		glBindAttribLocation(shaderProgram, 1, "vertexTexCoord");
		glBindAttribLocation(shaderProgram, 2, "vertexNormal");

		glBindFragDataLocation(shaderProgram, 0, "fragmentColor");

		glLinkProgram(shaderProgram);
		checkLinking(shaderProgram);
	}

	void UploadSamplerID()
	{
		int samplerUnit = 0; 
		int location = glGetUniformLocation(shaderProgram, "samplerUnit");
		glUniform1i(location, samplerUnit);
		glActiveTexture(GL_TEXTURE0 + samplerUnit); 
	}

	void UploadInvM(mat4& InvM)
	{
		int location = glGetUniformLocation(shaderProgram, "InvM");
		if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, InvM); 
		else printf("uniform InvM cannot be set\n");
	}

	void UploadMVP(mat4& MVP)
	{
		int location = glGetUniformLocation(shaderProgram, "MVP");
		if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, MVP); 
		else printf("uniform MVP cannot be set\n");
	}

	void UploadM(mat4& M)
	{
		int location = glGetUniformLocation(shaderProgram, "M");
		if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M); 
		else printf("uniform M cannot be set\n");
	}

    void UploadMaterialAttributes(vec3& ka, vec3& kd, vec3& ks, float shininess) {

        int location = glGetUniformLocation(shaderProgram, "ka");
		if (location >= 0) glUniform3f(location, ka.x, ka.y, ka.z); 
		else printf("uniform ka cannot be set\n");

        location = glGetUniformLocation(shaderProgram, "kd");
		if (location >= 0) glUniform3f(location, kd.x, kd.y, kd.z);
		else printf("uniform kd cannot be set\n");

        location = glGetUniformLocation(shaderProgram, "ks");
		if (location >= 0) glUniform3f(location, ks.x, ks.y, ks.z); 
		else printf("uniform ks cannot be set\n");

        location = glGetUniformLocation(shaderProgram, "shininess");
		if (location >= 0) glUniform1f(location, shininess);
		else printf("uniform shininess cannot be set\n");

    }

    void UploadLightAttributes(vec3& La, vec3& Le, vec4& worldLightPosition) {

        int location = glGetUniformLocation(shaderProgram, "La");
		if (location >= 0) glUniform3f(location, La.x, La.y, La.z);
		else printf("uniform La cannot be set\n");

        location = glGetUniformLocation(shaderProgram, "Le");
		if (location >= 0) glUniform3f(location, Le.x, Le.y, Le.z);
		else printf("uniform Le cannot be set\n");

        location = glGetUniformLocation(shaderProgram, "worldLightPosition");
		if (location >= 0) glUniform4f(location, worldLightPosition.x, 
                worldLightPosition.y, worldLightPosition.z, worldLightPosition.w);
		else printf("uniform worldLightPosition cannot be set\n");
    }

    void UploadEyePosition(vec3& wEye) {

        int location = glGetUniformLocation(shaderProgram, "worldEyePosition");
		if (location >= 0) glUniform3f(location, wEye.x, wEye.y, wEye.z);
		else printf("uniform wEye cannot be set\n");
    }
};

class InfiniteQuadShader: public Shader
{
public:
    InfiniteQuadShader() {
        const char *vertexSource = "\n\
        #version 130 \n\
        precision highp float; \n\
        \n\
        in vec4 vertexPosition; \n\
        in vec2 vertexTexCoord; \n\
        in vec3 vertexNormal; \n\
        uniform mat4 M, InvM, MVP; \n\
        \n\
        out vec2 texCoord; \n\
        out vec4 worldPosition; \n\
        out vec3 worldNormal; \n\
        \n\
        void main() { \n\
        texCoord = vertexTexCoord; \n\
        worldPosition = vertexPosition * M; \n\
        worldNormal = (InvM * vec4(vertexNormal, 0.0)).xyz; \n\
        gl_Position = vertexPosition * MVP; \n\
        }";

        const char* fragmentSource = "\n\
        #version 130 \n\
        precision highp float; \n\
        uniform sampler2D samplerUnit; \n\
        uniform vec3 La, Le; \n\
        uniform vec3 ka, kd, ks; \n\
        uniform float shininess; \n\
        uniform vec3 worldEyePosition; \n\
        uniform vec4 worldLightPosition; \n\
        in vec2 texCoord; \n\
        in vec4 worldPosition; \n\
        in vec3 worldNormal; \n\
        out vec4 fragmentColor; \n\
        void main() { \n\
        vec3 N = normalize(worldNormal); \n\
        vec3 V = normalize(worldEyePosition * worldPosition.w - worldPosition.xyz); \n\
        vec3 L = normalize(worldLightPosition.xyz * worldPosition.w - worldPosition.xyz * worldLightPosition.w); \n\
        vec3 H = normalize(V + L); \n\
        vec2 position = worldPosition.xz / worldPosition.w; \n\
        vec2 tex = position.xy - floor(position.xy); \n\
        vec3 texel = texture(samplerUnit, tex).xyz; \n\
        vec3 color = La * ka + Le * kd * texel * max(0.0, dot(L, N)) + Le * ks * pow(max(0.0, dot(H, N)), shininess); \n\
        fragmentColor = vec4(color, 1); \n\
        }";

		unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
		if (!vertexShader) { printf("Error in vertex shader creation\n"); exit(1); }

		glShaderSource(vertexShader, 1, &vertexSource, NULL);
		glCompileShader(vertexShader);
		checkShader(vertexShader, "Vertex shader error");

		unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		if (!fragmentShader) { printf("Error in fragment shader creation\n"); exit(1); }

		glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
		glCompileShader(fragmentShader);
		checkShader(fragmentShader, "Fragment shader error");

		shaderProgram = glCreateProgram();
		if (!shaderProgram) { printf("Error in shader program creation\n"); exit(1); }

		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);

		glBindAttribLocation(shaderProgram, 0, "vertexPosition");
		glBindAttribLocation(shaderProgram, 1, "vertexTexCoord");
		glBindAttribLocation(shaderProgram, 2, "vertexNormal");

		glBindFragDataLocation(shaderProgram, 0, "fragmentColor");

		glLinkProgram(shaderProgram);
		checkLinking(shaderProgram);
    }

	void UploadSamplerID()
	{
		int samplerUnit = 0; 
		int location = glGetUniformLocation(shaderProgram, "samplerUnit");
		glUniform1i(location, samplerUnit);
		glActiveTexture(GL_TEXTURE0 + samplerUnit); 
	}

	void UploadInvM(mat4& InvM)
	{
		int location = glGetUniformLocation(shaderProgram, "InvM");
		if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, InvM); 
		else printf("uniform InvM cannot be set\n");
	}

	void UploadMVP(mat4& MVP)
	{
		int location = glGetUniformLocation(shaderProgram, "MVP");
		if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, MVP); 
		else printf("uniform MVP cannot be set\n");
	}

	void UploadM(mat4& M)
	{
		int location = glGetUniformLocation(shaderProgram, "M");
		if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M); 
		else printf("uniform M cannot be set\n");
	}

    void UploadMaterialAttributes(vec3& ka, vec3& kd, vec3& ks, float shininess) {

        int location = glGetUniformLocation(shaderProgram, "ka");
		if (location >= 0) glUniform3f(location, ka.x, ka.y, ka.z); 
		else printf("uniform ka cannot be set\n");

        location = glGetUniformLocation(shaderProgram, "kd");
		if (location >= 0) glUniform3f(location, kd.x, kd.y, kd.z);
		else printf("uniform kd cannot be set\n");

        location = glGetUniformLocation(shaderProgram, "ks");
		if (location >= 0) glUniform3f(location, ks.x, ks.y, ks.z); 
		else printf("uniform ks cannot be set\n");

        location = glGetUniformLocation(shaderProgram, "shininess");
		if (location >= 0) glUniform1f(location, shininess);
		else printf("uniform shininess cannot be set\n");

    }

    void UploadLightAttributes(vec3& La, vec3& Le, vec4& worldLightPosition) {

        int location = glGetUniformLocation(shaderProgram, "La");
		if (location >= 0) glUniform3f(location, La.x, La.y, La.z);
		else printf("uniform La cannot be set\n");

        location = glGetUniformLocation(shaderProgram, "Le");
		if (location >= 0) glUniform3f(location, Le.x, Le.y, Le.z);
		else printf("uniform Le cannot be set\n");

        location = glGetUniformLocation(shaderProgram, "worldLightPosition");
		if (location >= 0) glUniform4f(location, worldLightPosition.x, 
                worldLightPosition.y, worldLightPosition.z, worldLightPosition.w);
		else printf("uniform worldLightPosition cannot be set\n");
    }

    void UploadEyePosition(vec3& wEye) {

        int location = glGetUniformLocation(shaderProgram, "worldEyePosition");
		if (location >= 0) glUniform3f(location, wEye.x, wEye.y, wEye.z);
		else printf("uniform wEye cannot be set\n");
    }

};


class ShadowShader: public Shader
{
public:
	ShadowShader()
	{
        const char *vertexSource = "\n\
        #version 130 \n\
        precision highp float; \n\
        \n\
        in vec3 vertexPosition; \n\
        in vec2 vertexTexCoord; \n\
        in vec3 vertexNormal; \n\
        uniform mat4 M, VP; \n\
        uniform vec4 worldLightPosition; \n\
        \n\
        void main() { \n\
        vec4 p = vec4(vertexPosition, 1) * M; \n\
        vec3 s; \n\
        s.y = -0.999; \n\
        s.x = (p.x - worldLightPosition.x) / (p.y - worldLightPosition.y) * (s.y - worldLightPosition.y) + worldLightPosition.x; \n\
        s.z = (p.z - worldLightPosition.z) / (p.y - worldLightPosition.y) * (s.y - worldLightPosition.y) + worldLightPosition.z; \n\
        gl_Position = vec4(s, 1) * VP; \n\
        }";

		const char *fragmentSource = "\n\
        #version 130 \n\
        precision highp float; \n\
        \n\
        out vec4 fragmentColor; \n\
        \n\
        void main() \n\
        { \n\
            fragmentColor = vec4(0.0, 0.1, 0.0, 1); \n\
        }";


		unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
		if (!vertexShader) { printf("Error in vertex shader creation\n"); exit(1); }

		glShaderSource(vertexShader, 1, &vertexSource, NULL);
		glCompileShader(vertexShader);
		checkShader(vertexShader, "Vertex shader error");

		unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		if (!fragmentShader) { printf("Error in fragment shader creation\n"); exit(1); }

		glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
		glCompileShader(fragmentShader);
		checkShader(fragmentShader, "Fragment shader error");

		shaderProgram = glCreateProgram();
		if (!shaderProgram) { printf("Error in shader program creation\n"); exit(1); }

		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);

		glBindAttribLocation(shaderProgram, 0, "vertexPosition");
		glBindAttribLocation(shaderProgram, 1, "vertexTexCoord");
		glBindAttribLocation(shaderProgram, 2, "vertexNormal");

		glBindFragDataLocation(shaderProgram, 0, "fragmentColor");

		glLinkProgram(shaderProgram);
		checkLinking(shaderProgram);
	}

    void UploadVP(mat4& VP) { 
		int location = glGetUniformLocation(shaderProgram, "VP");
		if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, VP); 
		else printf("uniform VP cannot be set\n");
	}

	void UploadM(mat4& M)
	{
		int location = glGetUniformLocation(shaderProgram, "M");
		if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M); 
		else printf("uniform M cannot be set\n");
	}


    void UploadLightAttributes(vec3& La, vec3& Le, vec4& worldLightPosition) {

        int location = glGetUniformLocation(shaderProgram, "worldLightPosition");
		if (location >= 0) glUniform4f(location, worldLightPosition.x, 
                worldLightPosition.y, worldLightPosition.z, worldLightPosition.w);
		else printf("uniform worldLightPosition cannot be set\n");
    }

};


extern "C" unsigned char* stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);

class Texture
{
	unsigned int textureId;

public:
	Texture(const std::string& inputFileName)
	{
		unsigned char* data;
		int width; int height; int nComponents = 4;
		
		data = stbi_load(inputFileName.c_str(), &width, &height, &nComponents, 0);

		if(data == NULL) 
		{ 
			return;
		}

		glGenTextures(1, &textureId); 
		glBindTexture(GL_TEXTURE_2D, textureId); 
		
		if(nComponents == 3) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		if(nComponents == 4) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		delete data; 
	}

	void Bind()
	{
		glBindTexture(GL_TEXTURE_2D, textureId);
	}
};



class Material
{
	Shader* shader;
	Texture* texture;
    vec3 ka, kd, ks;
    float shininess;

public:
	Material(Shader* sh, Texture* t = 0, 
             vec3 a=vec3(0.1,0.1,0.1), vec3 d=vec3(0.9,0.9,0.9), 
             vec3 s=vec3(0.0,0.0,0.0), float shine=20.0)
	{
		shader = sh;
		texture = t;
        ka = a; kd=d; ks=s;
        shininess = shine;
	}

	Shader* GetShader() { return shader; }

	void UploadAttributes()
	{
		if(texture)
		{
            shader->UploadMaterialAttributes(ka, kd, ks, shininess);
			shader->UploadSamplerID();
			texture->Bind();
		}
		else {
        }
    }
};


class Light
{
    vec3 La, Le;
    vec4 worldLightPosition;

public:
    Light(vec3 a, vec3 e, vec4 worldLight) {
        La = a;
        Le = e;
        worldLightPosition = worldLight;
    }

    void UploadAttributes(Shader* shader) {
        shader->UploadLightAttributes(La, Le, worldLightPosition);
    }

    void SetPointLightSource(vec3& pos) {
        worldLightPosition = vec4(pos.x, pos.y, pos.z, 1);
    }

    void SetDirectionalLightSource(vec3& dir) {
        worldLightPosition = vec4(dir.x, dir.y, dir.z, 0);
    }

};

Light* light;


class Mesh
{
	Geometry* geometry;
	Material* material;

public:
	Mesh(Geometry* g, Material* m)
	{
		geometry = g;
		material = m;
	}

	Shader* GetShader() { return material->GetShader(); }

	void Draw()
	{
		material->UploadAttributes();
		geometry->Draw();
	}
};





class Camera {
   float fov, asp, fp, bp;
   float velocity, angularVelocity;

public:
   std::string state;
   vec3  wEye, wLookat, wVup, wAvi;
   float alpha;
	Camera()
	{
        state = "followKeys";
		wEye = vec3(0.0, 0.0, 2.0);
		wLookat = vec3(0.0, 0.0, 0.0);
        wAvi = vec3(0.0, 0.0, 0.2);
		wVup = vec3(0.0, 1.0, 0.0);
		fov = M_PI / 4.0; asp = 1.0; fp = 0.01; bp = 10.0;		
        velocity = 0.0; angularVelocity = 0.0;
	}
	
	void SetAspectRatio(float a) { asp = a; }

	mat4 GetViewMatrix() 
	{ 
		vec3 w = (wEye - wLookat).normalize();
		vec3 u = cross(wVup, w).normalize();
		vec3 v = cross(w, u);
	
		return  
			mat4(	
				1.0f,    0.0f,    0.0f,    0.0f,
				0.0f,    1.0f,    0.0f,    0.0f,
				0.0f,    0.0f,    1.0f,    0.0f,
				-wEye.x, -wEye.y, -wEye.z, 1.0f ) *
			mat4(	
				u.x,  v.x,  w.x,  0.0f,
				u.y,  v.y,  w.y,  0.0f,
				u.z,  v.z,  w.z,  0.0f,
				0.0f, 0.0f, 0.0f, 1.0f );
   }

	mat4 GetProjectionMatrix() 
	{ 
		float sy = 1/tan(fov/2);
		return mat4(
			sy/asp, 0.0f,  0.0f,               0.0f,
			0.0f,   sy,    0.0f,               0.0f,
			0.0f,   0.0f, -(fp+bp)/(bp - fp), -1.0f,
			0.0f,   0.0f, -2*fp*bp/(bp - fp),  0.0f);
	}

    void UploadAttributes(Shader* shader) {
        shader->UploadEyePosition(wEye);
    }

    void Control() {
        // trace mode
         if (keyboardState['t'] == true) {
             state = "heartTrace";
         } if (keyboardState['h'] == true) {
             state = "heliCam";
         } if (keyboardState['c'] == true) {
             state = "followKeys";
         }

         // rotate negative angular velocity
         if (keyboardState['a'] == true) {
             angularVelocity = -0.5;
         // rotate positive angular velocity (with dvorak keyboard)
         } else if (keyboardState['e'] == true) {
             angularVelocity = 0.5;
         } else { angularVelocity = 0.0; }
         // move forward (with dvorak keyboard)
         if (keyboardState[','] ==  true) {
             velocity = 2.0;
         // move backward (with dvorak keyboard)
         } else if (keyboardState['o'] == true) {
             velocity = -2.0;
         } else { velocity = 0.0; }
    }

    float len(vec3 a, vec3 b) {
        return sqrt( pow(a.x - b.x, 2) + pow(a.y - b.y, 2) + pow(a.z - b.z, 2));
    }

    /* return the direction from a to be on x y z (1 or -1) */
    vec3 sign(vec3 a, vec3 b) {
        float x;
        float y;
        float z;
        if (a.x > b.x) {x = 1.0;} else {x = -1.0;}
        if (a.y > b.y) {y = 1.0;} else {y = -1.0;}
        if (a.z > b.z) {z = 1.0;} else {z = -1.0;}
        return vec3(x,y,z);
    }

    float rad_2_deg(float rad) {
        return rad * 180 / 3.14159;
    }

    float sign(float x) {
        if ((x) < 0.0) {
            return -1.0;
        } else {
            return 1.0;
        }
    }
    
    void Move(float dt) {
        if (state == "followKeys") {
            vec3 norm = (wLookat - wEye).normalize();
            float beta = angularVelocity * dt;
            float l = len(wLookat, wEye);
            mat4 rot = mat4(
                    cos(beta), 0, sin(beta), 0,
                    0, 1, 0, 0,
                    -sin(beta), 0, cos(beta), 0,
                    0, 0, 0, 1);
            vec4 newNorm4 = vec4(norm.x, norm.y, norm.z, 1) * rot;

            wEye = wEye + vec3(newNorm4.v[0], newNorm4.v[1], newNorm4.v[2])*velocity*dt;
            wLookat = wEye + vec3(newNorm4.v[0], newNorm4.v[1], newNorm4.v[2]) * l;
            wAvi = wEye + vec3(newNorm4.x, newNorm4.y, newNorm4.z)*l*0.5;
            vec3 zaxis = vec3(0, 0, 1);
            float dotp = dot(norm, zaxis);
            alpha = -1 * sign(wLookat.x-wEye.x) *  rad_2_deg(acos(dotp));
        } else if (state == "heliCam") {
        } else if (state == "heartTrace") {

        }
    }

};

Camera camera;


class Object
{
	Mesh *mesh;
	Shader* mShader;

	vec3 scaling;

public:
	float orientation;
	vec3 position;
	Object(Mesh *m, vec3 position = vec3(0.0, 0.0, 0.0), vec3 scaling = vec3(1.0, 1.0, 1.0), float orientation = 0.0) : position(position), scaling(scaling), orientation(orientation)
	{
		mShader = m->GetShader();
		mesh = m;
	}

	vec3& GetPosition() { return position; }

	void Draw()
	{
		mShader->Run();
        light->UploadAttributes(mShader);
		UploadAttributes();
		mesh->Draw();
	}

    void DrawShadow(Shader* shadowShader) {
        shadowShader->Run();
        UploadAttributes(shadowShader);

        vec3 pos = vec3(100.0, 100.0, 100.0);
        light->SetPointLightSource( pos );
        light->UploadAttributes(shadowShader);

        camera.UploadAttributes(shadowShader);

        mesh->Draw();

    }

	void UploadAttributes(Shader *shader=0)
	{
        if (shader == 0) {
            shader = mShader;
        }
		mat4 T = mat4(
			1.0,			0.0,			0.0,			0.0,
			0.0,			1.0,			0.0,			0.0,
			0.0,			0.0,			1.0,			0.0,
			position.x,		position.y,		position.z,		1.0);

		mat4 InvT = mat4(
			1.0,			0.0,			0.0,			0.0,
			0.0,			1.0,			0.0,			0.0,
			0.0,			0.0,			1.0,			0.0,
			-position.x,	-position.y,	-position.z,	1.0);

		mat4 S = mat4(
			scaling.x,		0.0,			0.0,			0.0,
			0.0,			scaling.y,		0.0,			0.0,
			0.0,			0.0,			scaling.z,		0.0,
			0.0,			0.0,			0.0,			1.0);

		mat4 InvS = mat4(
			1.0/scaling.x,	0.0,			0.0,			0.0,
			0.0,			1.0/scaling.y,	0.0,			0.0,
			0.0,			0.0,			1.0/scaling.z,	0.0,
			0.0,			0.0,			0.0,			1.0);

		float alpha = orientation / 180.0 * M_PI;

		mat4 R = mat4(
			cos(alpha),		0.0,			sin(alpha),		0.0,
			0.0,			1.0,			0.0,			0.0,
			-sin(alpha),	0.0,			cos(alpha),		0.0,
			0.0,			0.0,			0.0,			1.0);

		mat4 InvR = mat4(
			cos(alpha),		0.0,			-sin(alpha),	0.0,
			0.0,			1.0,			0.0,			0.0,
			sin(alpha),		0.0,			cos(alpha),		0.0,
			0.0,			0.0,			0.0,			1.0);

		mat4 M = S * R * T;
		mat4 InvM = InvT * InvR * InvS;

		mat4 MVP = M * camera.GetViewMatrix() * camera.GetProjectionMatrix();
        mat4 VP = camera.GetViewMatrix() * camera.GetProjectionMatrix();

        shader->UploadM(M);
		shader->UploadInvM(InvM);
		shader->UploadMVP(MVP);
        shader->UploadVP(VP);
	}
};

class Chevy
{
    Object* chassis;
    Object* wheel;
    Light* spotlight;
    float wheelAngle;
    float velocity, angularVelocity;
public:
    Chevy(Object* chassis, Object* wheel, Light* spotlight): chassis(chassis), wheel(wheel), spotlight(spotlight)
    {
    }

    void Control() {
     // rotate negative angular velocity
     if (keyboardState['a'] == true) {
         angularVelocity = -100;
     // rotate positive angular velocity (with dvorak keyboard)
     } else if (keyboardState['e'] == true) {
         angularVelocity = 100;
     } else { angularVelocity = 0.0;}
     // move forward (with dvorak keyboard)
     if (keyboardState[','] ==  true) {
         velocity = 0.02;
     // move backward (with dvorak keyboard)
     } else if (keyboardState['o'] == true) {
         velocity = -0.02;
     } else { velocity = 0.0; }
    }

    vec3 dirs(float angle) {
        vec3 d = vec3(0, 0, 0);
        angle = ( (int) angle % 360);
        if ( (angle < 90) or (angle > 270)) {
            d.z = 1;
        } else {
            d.z = -1;
        }
        if ( (angle < 180) ) {
            d.x = -1;
        } else {
            d.x = 1;
        }
        return d;
    }

    void Draw(float dt) {
        if (camera.state == "followKeys") {
            chassis->position = vec3(camera.wLookat.x, chassis->position.y, camera.wLookat.z);
            chassis->orientation = camera.alpha;
        } else if (camera.state == "heliCam") {
            float dAngle = angularVelocity*dt;
            float ori = chassis->orientation;
            vec3 dir = dirs(ori);
            vec3 pos = chassis->position;
            printf("%f\n", chassis->orientation);
            chassis->position = vec3(pos.x + dir.x*sin(3.14/180*chassis->orientation)*velocity, pos.y, pos.z+dir.z*cos(3.14/180*chassis->orientation)*velocity);
            chassis->orientation = chassis->orientation + angularVelocity*dt;
            camera.wEye = vec3(pos.x - dir.x*sin(3.14/180*ori)*2, camera.wEye.y, pos.z - dir.z*cos(3.14/180*ori)*2);
            camera.wLookat = chassis->position;

        }
            vec3 lPos = vec3(chassis->position.x, chassis->position.y+100, chassis->position.z);
            light->SetPointLightSource(lPos);
            chassis->Draw();

        
    }

};

class Scene
{
	MeshShader *meshShader;
    InfiniteQuadShader *groundShader;
    ShadowShader *shadowShader;

    Object* avi;
    Object* gnd;
    Chevy* chevy;
	
	std::vector<Texture*> textures;
	std::vector<Material*> materials;
	std::vector<Geometry*> geometries;
	std::vector<Mesh*> meshes;
	std::vector<Object*> objects;

public:
	Scene() 
	{ 
		meshShader = 0;
        groundShader = 0;
        shadowShader = 0;

	}

	void Initialize()
	{
		meshShader = new MeshShader();
        groundShader = new InfiniteQuadShader();
        shadowShader = new ShadowShader();

        // make tigger
		textures.push_back(new Texture("tigger.png"));
		materials.push_back(new Material(meshShader, textures[0], vec3(0.1, 0.1, 0.1),
                            vec3(0.6,0.6,0.6), vec3(0.3, 0.3, 0.3), 50)); 
		geometries.push_back(new PolygonalMesh("tigger.obj"));		
		meshes.push_back(new Mesh(geometries[0], materials[0]));
		
		Object* object = new Object(meshes[0], vec3(2.0, -1.0, -3.0), vec3(0.05, 0.05, 0.05), -90.0);
		objects.push_back(object);

        // make tree
		textures.push_back(new Texture("tree.png"));
		materials.push_back(new Material(meshShader, textures[1], vec3(0.1, 0.1, 0.1), 
                            vec3(0.9,0.9,0.9), vec3(0.0, 0.0, 0.0), 50)); 
		geometries.push_back(new PolygonalMesh("tree.obj"));		
		meshes.push_back(new Mesh(geometries[1], materials[1]));
		
		Object* ob = new Object(meshes[1], vec3(-0.8, 0.0, 0.0), vec3(0.025, 0.025, 0.025), 0.0);
		objects.push_back(ob);

        // make the second tree
        objects.push_back( new Object(meshes[1], vec3(0.0, 0.0, 3.0), vec3(0.025, 0.025, 0.025), 0.0));

        // make floor
        materials.push_back(new Material(groundShader, textures[1], vec3(0.1, 0.1, 0.1),
                            vec3(0.6, 0.6, 0.6), vec3(0.3, 0.3, 0.3), 50));
        geometries.push_back(new TexturedQuad());
        meshes.push_back(new Mesh(geometries[2], materials[2]));
        gnd = (new Object(meshes[2], vec3(0.0, -1.0, 0.0), vec3(1.0, 1.0, 1.0), 0));

        // avatar (chevy)
        textures.push_back(new Texture("chevy/chevy.png"));
        materials.push_back(new Material(meshShader, textures[2]));
        geometries.push_back(new PolygonalMesh("chevy/chassis.obj"));
        meshes.push_back(new Mesh(geometries[3], materials[3]));
        avi = new Object(meshes[3], vec3(0.0, -0.5, 0.9), vec3(0.03, 0.03, 0.03), 180);
        
        geometries.push_back(new PolygonalMesh("chevy/wheel.obj"));
        meshes.push_back(new Mesh(geometries[4], materials[3]));
        avi = new Object(meshes[3], vec3(0.0, -0.5, 0.9), vec3(0.03, 0.03, 0.03), 180);
        Object* wheel = new Object(meshes[4], vec3(0.0, -0.5, 0.9), vec3(0.03, 0.03, 0.03), 180);

        Light* chevLight = new Light(vec3(1.5, 1.5, 1.5), vec3(1.5, 1.5, 1.5), vec4(avi->position.x, avi->position.y, avi->position.z, 1));
        chevy = new Chevy(avi, wheel, chevLight);
        objects.push_back(avi);


	}

	~Scene()
	{
		for(int i = 0; i < textures.size(); i++) delete textures[i];
		for(int i = 0; i < materials.size(); i++) delete materials[i];
		for(int i = 0; i < geometries.size(); i++) delete geometries[i];
		for(int i = 0; i < meshes.size(); i++) delete meshes[i];
		for(int i = 0; i < objects.size(); i++) delete objects[i];
		
		if(meshShader) delete meshShader;
	}

	void Draw(float dt=0.0)
	{
        gnd->Draw();
        chevy->Control();
        chevy->Draw(dt);
		for(int i = 0; i < objects.size(); i++) 
        {
            objects[i]->Draw();
            objects[i]->DrawShadow(shadowShader);
        }
	}
};

Scene scene;

void onInitialization() 
{
    light = new Light(vec3(.5, .5, .5), vec3(1.5, 1.5, 1.5), vec4(-7.0, 1.0, 20.0, 0.0));
	glViewport(0, 0, windowWidth, windowHeight);

	scene.Initialize();
}

void onExit() 
{
	printf("exit");
}

void onDisplay() 
{	
	
	glClearColor(0, 0, 1.0, 0); 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
	
	scene.Draw();

	glutSwapBuffers(); 
	
}

void onKeyboard(unsigned char key, int x, int y)
{
	keyboardState[key] = true;
}

void onKeyboardUp(unsigned char key, int x, int y)
{
	keyboardState[key] = false;
}

void onSpecialInput(int key, int x, int y) {
    switch(key) {
        case GLUT_KEY_UP:
        break;

        case GLUT_KEY_DOWN:
        break;

        case GLUT_KEY_LEFT:
        break;

        case GLUT_KEY_RIGHT:
        break;
    }
}

void onReshape(int winWidth, int winHeight) 
{
	camera.SetAspectRatio((float)winWidth / winHeight);
	glViewport(0, 0, winWidth, winHeight);
}

void onIdle( ) {
    double t = glutGet(GLUT_ELAPSED_TIME) * 0.001;
    static double lastTime = 0.0;
    double dt = t - lastTime;
    lastTime = t;

    camera.Control();
    camera.Move(dt);
    scene.Draw(dt);

    glutPostRedisplay();
}

int main(int argc, char * argv[]) 
{
	glutInit(&argc, argv);
#if !defined(__APPLE__)
	glutInitContextVersion(majorVersion, minorVersion);
#endif
	glutInitWindowSize(windowWidth, windowHeight); 
	glutInitWindowPosition(50, 50);
#if defined(__APPLE__)
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_3_2_CORE_PROFILE);  
#else
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
	glutCreateWindow("3D Mesh Rendering");

#if !defined(__APPLE__)
	glewExperimental = true;	
	glewInit();
#endif
	printf("GL Vendor    : %s\n", glGetString(GL_VENDOR));
	printf("GL Renderer  : %s\n", glGetString(GL_RENDERER));
	printf("GL Version (string)  : %s\n", glGetString(GL_VERSION));
	glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
	glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
	printf("GL Version (integer) : %d.%d\n", majorVersion, minorVersion);
	printf("GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	
	onInitialization();

	glutDisplayFunc(onDisplay); 
	glutIdleFunc(onIdle);
	glutKeyboardFunc(onKeyboard);
	glutKeyboardUpFunc(onKeyboardUp);
    glutSpecialFunc(onSpecialInput);
	glutReshapeFunc(onReshape);

	glutMainLoop();
	onExit();
	return 1;
}


