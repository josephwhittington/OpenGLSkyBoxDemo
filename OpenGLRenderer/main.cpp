#define STB_IMAGE_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <vector>

#include <GL\glew.h>
#include <GLFW\glfw3.h>

#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>

#include "CommonValues.h"

#include "WindowUtility.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "Spotlight.h"
#include "Material.h"

#include "Model.h"
#include "SkyBox.h"

const float toRadians = 3.1415265f / 180.0f;

GLuint uniformModel = 0, uniformProjection = 0, uniformView = 0, uniformEyePosition = 0,
uniformSpecularIntensity = 0, uniformShininess = 0, uniformOmniLightPos = 0, uniformFarPlane = 0;

WindowUtility mainWindow;
std::vector<Mesh*> meshList;

std::vector<Shader> shaderList;
Shader directionalShadowShader;
Shader omniShadowShader;

Camera camera;

Texture brickTexture;
Texture dirtTexture;
Texture plainTexture;

Material shinyMaterial;
Material dullMaterial;

Model xwing;
Model blackHawk;

DirectionalLight mainLight;
PointLight pointLights[MAX_POINT_LIGHTS];
SpotLight spotLights[MAX_SPOT_LIGHTS];

Skybox skybox;

unsigned int pointLightCount = 0;
unsigned int spotLightCount = 0;

GLfloat deltaTime = 0.0f;
GLfloat lastTime = 0.0f;

GLfloat blackhawkAngle = 0.0f;

// Vertex Shader 
static const char* vShader = "Shaders/shader.vert";

// Fragment Shader
static const char* fShader = "Shaders/shader.frag";

// Bruh
#define G_EPSILON_F                    1.192092896e-07F
#define G_FIRST_COMPARISON_F(num1 , num2)   ( G_ABS( (num1) - (num2) ) <= ( G_EPSILON_F ) )    //FIRST CHECK IF TWO INPUT FLOATS' DIFF ARE LESS THAN EPSILON
#define G_SECOND_COMPARISON_F(num1 , num2)   ( G_ABS( (num1) - (num2) ) <= ( G_DEVIATION_STANDARD_F * G_ABS_LARGER(num1, num2)) )     //SECOND CHECK IF TWO INPUT FLOATS' DIFF ARE LESS THAN EPSILON MULTIPLY THE LARGER INPUT FLOAT
#define G_COMPARISON_STANDARD_F(num1 , num2)   ( G_FIRST_COMPARISON_F( (num1), (num2) ) ? true : G_SECOND_COMPARISON_F( (num1) , (num2) ) )
#define G_COMPARISON_F(num1, num2, deviation) ( G_FIRST_COMPARISON_F( (num1), (num2) )    ? true :  ( G_ABS( (num1) - (num2) ) <=  ( deviation * G_ABS_LARGER(num1, num2)) )  )

#define G_DEVIATION_STANDARD_F G_EPSILON_F * 10
#define G_ABS(num)                    ( ( (num) > 0 ) ?  (num) : (-(num)) )        //RETURN THE ABSOLUTE VALUE OF THE INPUT NUMBER
#define G_LARGER(A, B)                ( ( (A) > (B) ) ?  (A) : (B) )            //RETURN THE LARGER INPUT NUMBER
#define G_SMALLER(A, B)                ( ( (A) < (B) ) ?  (A) : (B) )            //RETURN THE SMALLER INPUT NUMBER
#define G_ABS_LARGER(A, B)        ( ( G_ABS(A) > G_ABS(B) ) ?  G_ABS(A) : G_ABS(B) )

struct wmat4
{
	struct { float data[4]; } row1;
	struct { float data[4]; } row2;
	struct { float data[4]; } row3;
	struct { float data[4]; } row4;
};

// Bruh

static bool ProjectionRHF(float _fovY, float _aspect, float _zn, float _zf, wmat4& _outMatrix)
{
	if (G_COMPARISON_STANDARD_F(_aspect, 0.0f)) return false;

	float yScale = 1.0f / tanf(_fovY * 0.5f);
	float z = 1.0f / (_zf - _zn);

	_outMatrix = {};
	_outMatrix.row1.data[0] = yScale / _aspect;
	_outMatrix.row2.data[1] = yScale;
	_outMatrix.row3.data[2] = -(_zf + _zn) * z;
	_outMatrix.row3.data[3] = (-2.0f * _zf * _zn) * z;
	_outMatrix.row4.data[2] = -1;

	return true;
}

void CalcAverageNormals(unsigned int* indices, unsigned int indexCount, GLfloat* vertices,
	unsigned int vertexCount, unsigned int vLength, unsigned int normalOffset)
{
	for (size_t i = 0; i < indexCount; i += 3)
	{
		unsigned int in0 = indices[i] * vLength;
		unsigned int in1 = indices[i + 1] * vLength;
		unsigned int in2 = indices[i + 2] * vLength;
		glm::vec3 v1(vertices[in1] - vertices[in0], vertices[in1 + 1] - vertices[in0 + 1], vertices[in1 + 2] - vertices[in0 + 2]);
		glm::vec3 v2(vertices[in2] - vertices[in0], vertices[in2 + 1] - vertices[in0 + 1], vertices[in2 + 2] - vertices[in0 + 2]);
		glm::vec3 normal = glm::cross(v1, v2);
		normal = glm::normalize(normal);

		in0 += normalOffset; in1 += normalOffset; in2 += normalOffset;
		vertices[in0] += normal.x; vertices[in0 + 1] += normal.y; vertices[in0 + 2] += normal.z;
		vertices[in1] += normal.x; vertices[in1 + 1] += normal.y; vertices[in1 + 2] += normal.z;
		vertices[in2] += normal.x; vertices[in2 + 1] += normal.y; vertices[in2 + 2] += normal.z;
	}

	for (size_t i = 0; i < vertexCount / vLength; i++)
	{
		unsigned int nOffset = i * vLength + normalOffset;
		glm::vec3 vec(vertices[nOffset], vertices[nOffset + 1], vertices[nOffset + 2]);
		vec = glm::normalize(vec);
		vertices[nOffset] = vec.x; vertices[nOffset + 1] = vec.y; vertices[nOffset + 2] = vec.z;
	}
}

void CreateObjects()
{
	unsigned int indices[] =
	{
		0, 3, 1,
		1, 3, 2,
		2, 3, 0,
		0, 1, 2
	};

	GLfloat vertices[] = {
		// x       y      z     u      v          nx    ny    nz
		-1.0f, -1.0f, 0.0f,    0.0f, 0.0f,      0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 1.0f,     0.5f, 0.0f,      0.0f, 0.0f, 0.0f,
		1.0f, -1.0f, 0.0f,     1.0f, 0.0f,      0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,      0.5f, 1.0f,		0.0f, 0.0f, 0.0f
	};

	unsigned int floorIndices[] = {
		0, 2, 1,
		1, 2, 3
	};

	GLfloat floorVertices[] = {
		-10.0f, 0.0f, -10.0f,    0.0f, 0.0f,     0.0f, -1.0f, 0.0f,
		10.0f, 0.0f, -10.0f,     10.0f, 0.0f,    0.0f, -1.0f, 0.0f,
		-10.0f, 0.0f, 10.0f,     0.0f, 10.0f,	 0.0f, -1.0f, 0.0f,
		10.0f, 0.0f, 10.0f,       10.0f, 10.0f,   0.0f, -1.0f, 0.0f
	};

	CalcAverageNormals(indices, 12, vertices, 32, 8, 5);

	Mesh* obj1 = new Mesh();
	obj1->CreateMesh(vertices, indices, 32, 12);
	meshList.push_back(obj1);
	
	Mesh* obj2 = new Mesh();
	obj2->CreateMesh(vertices, indices, 32, 12);
	meshList.push_back(obj2);
	
	Mesh* obj3 = new Mesh();
	obj3->CreateMesh(floorVertices, floorIndices, 32, 6);
	meshList.push_back(obj3);
}

void CreateShaders()
{
	Shader* Shader1 = new Shader();
	Shader1->CreateFromFiles(vShader, fShader);
	shaderList.push_back(*Shader1);

	directionalShadowShader = Shader();
	directionalShadowShader.CreateFromFiles("Shaders/directional_shadow_map.vert", "Shaders/directional_shadow_map.frag");
	omniShadowShader.CreateFromFiles("Shaders/omni_shadow_map.vert", "Shaders/omni_shadow_map.geom", "Shaders/omni_shadow_map.frag");
}

void RenderScene()
{
	glm::mat4 model(1.0f);

	model = glm::translate(model, glm::vec3(0.0f, 0.0f, -2.5f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	brickTexture.UseTexture();
	shinyMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	meshList[0]->RenderMesh();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(0.0f, 4.0f, -2.5f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dirtTexture.UseTexture();
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	meshList[1]->RenderMesh();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(0.0f, -2.0f, 0.0f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dirtTexture.UseTexture();
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	meshList[2]->RenderMesh();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-7.0f, -0.0f, 10.0f));
	model = glm::scale(model, glm::vec3(0.006f, 0.006f, 0.006f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	shinyMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	xwing.RenderModel();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-3.0f, 2.0f, 0.0f));
	model = glm::scale(model, glm::vec3(0.04f, 0.04f, 0.04f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	shinyMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	blackHawk.RenderModel();
}

void DirectionalShadowMapPass(DirectionalLight* light)
{
	directionalShadowShader.UseShader();

	glViewport(0, 0, light->GetShadowMap()->GetShadowWidth(), light->GetShadowMap()->GetShadowHeight());

	light->GetShadowMap()->Write();
	glClear(GL_DEPTH_BUFFER_BIT);

	uniformModel = directionalShadowShader.GetModelLocation();
	directionalShadowShader.SetDirectionalLightTransform(&light->CalculateLightTransform());

	directionalShadowShader.Validate();
	RenderScene();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OmniShadowMapPass(PointLight* light)
{
	omniShadowShader.UseShader();

	glViewport(0, 0, light->GetShadowMap()->GetShadowWidth(), light->GetShadowMap()->GetShadowHeight());

	light->GetShadowMap()->Write();
	glClear(GL_DEPTH_BUFFER_BIT);

	uniformModel = omniShadowShader.GetModelLocation();
	uniformOmniLightPos = omniShadowShader.GetOmniLightPosLocation();
	uniformFarPlane = omniShadowShader.GetFarPlaneLocation();

	glUniform3f(uniformOmniLightPos, light->GetPosition().x, light->GetPosition().y, light->GetPosition().z);
	glUniform1f(uniformFarPlane, light->GetFarPlane());

	omniShadowShader.SetLightMatrices(light->CalculateLightTransform());

	omniShadowShader.Validate();
	RenderScene();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPass(glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
{
	glViewport(0, 0, 1366, 768);

	// Clear window
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	skybox.DrawSkybox(viewMatrix, projectionMatrix);

	shaderList[0].UseShader();

	uniformModel = shaderList[0].GetModelLocation();
	uniformProjection = shaderList[0].GetProjectionLocation();
	uniformView = shaderList[0].GetViewLocation();
	uniformModel = shaderList[0].GetModelLocation();
	uniformEyePosition = shaderList[0].GetEyePositionLocation();
	uniformSpecularIntensity = shaderList[0].GetEyePositionLocation();
	uniformShininess = shaderList[0].GetShininessLocation();

	glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(viewMatrix));
	glUniform3f(uniformEyePosition, camera.GetCameraPosition().x, camera.GetCameraPosition().y, camera.GetCameraPosition().z);

	shaderList[0].SetDirectionalLight(&mainLight);
	shaderList[0].SetPointLights(pointLights, pointLightCount, 3, 0);
	shaderList[0].SetSpotLights(spotLights, spotLightCount, 3 + pointLightCount, pointLightCount);
	shaderList[0].SetDirectionalLightTransform(&mainLight.CalculateLightTransform());

	mainLight.GetShadowMap()->Read(GL_TEXTURE2);
	shaderList[0].SetTexture(1);
	shaderList[0].SetDirectionalShadowMap(2);

	glm::vec3 lowerLight = camera.GetCameraPosition();
	lowerLight.y -= 0.3f;
	spotLights[0].SetFlash(lowerLight, camera.GetCameraDirection());

	shaderList[0].Validate();
	RenderScene();
}

int main()
{
	mainWindow = WindowUtility(1366, 768);
	mainWindow.Initialize();

	CreateObjects();
	CreateShaders();

	camera = Camera(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), -60.0f, 0.0f, 5.0f, 0.5f);

	brickTexture = Texture((char*)"Textures/brick.png");
	brickTexture.LoadTextureA();
	dirtTexture = Texture((char*)"Textures/dirt.png");
	dirtTexture.LoadTextureA();
	plainTexture = Texture((char*)"Textures/plain.png");
	plainTexture.LoadTextureA();

	shinyMaterial = Material(4.0f, 256);
	dullMaterial = Material(0.3f, 4);

	xwing = Model();
	xwing.LoadModel("Models/x-wing.obj");

	blackHawk = Model();
	blackHawk.LoadModel("Models/Seahawk.obj");

	mainLight = DirectionalLight(2048, 2048,
		1.0f, 1.0f, 1.0f,
		0.1f, 0.8f,
		0.0f, -15.0f, -10.0f);

	pointLights[1] = PointLight(1024, 1024,
		0.1f, 100.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.4f,
		2.0f, 2.0f, 0.0f,
		0.3f, 0.01f, 0.01f);
	//pointLightCount++;
	pointLights[0] = PointLight(1024, 1024,
		0.1f, 100.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.4f,
		-2.0f, 2.0f, 0.0f,
		0.3f, 0.01f, 0.01f);
	//pointLightCount++;


	spotLights[0] = SpotLight(1024, 1024,
		0.1f, 100.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 2.0f,
		0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		20.0f);
	spotLightCount++;
	spotLights[1] = SpotLight(1024, 1024,
		0.1f, 100.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, -1.5f, 0.0f,
		-100.0f, -1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		20.0f);
	spotLightCount++;

	std::vector<std::string> skyboxFaces;
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_rt.tga");
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_lf.tga");
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_up.tga");
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_dn.tga");
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_bk.tga");
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_ft.tga");

	skybox = Skybox(skyboxFaces);

	spotLights[1].SetFlash(camera.GetCameraPosition(), glm::vec3(1.0f, 0.0f, 0.0f));

	//glm::mat4 projection = glm::perspective(glm::radians(60.0f), (GLfloat)mainWindow.GetBufferWidth() / mainWindow.GetBufferHeight(), 0.1f, 100.0f);
	glm::mat4 projection;
	wmat4 eh;
	ProjectionRHF(glm::radians(60.0f), (float)mainWindow.GetBufferWidth() / mainWindow.GetBufferHeight(), 0.1f, 100.0f, eh);
	//memcpy(&projection, &eh, 16);
	// Bruh
	projection[0][0] = eh.row1.data[0];
	projection[1][0] = eh.row1.data[1];
	projection[2][0] = eh.row1.data[2];
	projection[3][0] = eh.row1.data[3];
	
	projection[0][1] = eh.row2.data[0];
	projection[1][1] = eh.row2.data[1];
	projection[2][1] = eh.row2.data[2];
	projection[3][1] = eh.row2.data[3];
	
	projection[0][2] = eh.row3.data[0];
	projection[1][2] = eh.row3.data[1];
	projection[2][2] = eh.row3.data[2];
	projection[3][2] = eh.row3.data[3];
	
	projection[0][3] = eh.row4.data[0];
	projection[1][3] = eh.row4.data[1];
	projection[2][3] = eh.row4.data[2];
	projection[3][3] = eh.row4.data[3];
	//Bruh

	// Loop until window closed
	while (!mainWindow.GetShouldClose())
	{
		GLfloat now = (GLfloat)glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		// Get + Handle user input events
		glfwPollEvents();
		
		// Camera control
		camera.keyControl(mainWindow.getKeys(), deltaTime);
		camera.mouseConrol(mainWindow.getXChange(), mainWindow.getYChange());

		if (mainWindow.getKeys()[GLFW_KEY_L])
		{
			spotLights[0].Toggle();
			mainWindow.getKeys()[GLFW_KEY_L] = false;
		}

		DirectionalShadowMapPass(&mainLight);
		for (size_t i = 0; i < pointLightCount; i++)
		{
			OmniShadowMapPass(&pointLights[i]);
		}
		for (size_t i = 0; i < spotLightCount; i++)
		{
			OmniShadowMapPass(&spotLights[i]);
		}
		RenderPass(camera.calculateViewMatrix(), projection);
		
		mainWindow.SwapBuffers();
	}

	return 0;
}