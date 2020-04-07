#pragma once

#include <stdio.h>
#include <GL\glew.h>
#include <GLFW\glfw3.h>

class WindowUtility
{
public:
	WindowUtility();
	WindowUtility(GLuint windowWidth, GLuint windowHeight);

	int Initialize();

	GLfloat GetBufferWidth() { return bufferWidth; }
	GLfloat GetBufferHeight() { return bufferHeight; }

	bool GetShouldClose() { return glfwWindowShouldClose(mainWindow); }

	bool* getKeys() { return keys; }
	GLfloat getXChange();
	GLfloat getYChange();

	void SwapBuffers() { glfwSwapBuffers(mainWindow); }

	~WindowUtility();

private:
	GLFWwindow* mainWindow;
	GLuint width, height;

	int bufferWidth, bufferHeight;

	bool keys[1024];

	GLfloat lastX;
	GLfloat lastY;
	GLfloat xChange;
	GLfloat yChange;
	bool mouseFirstMoved;

	// This is some callback fuckery for GLFW - look into simplifying this to mkae more sense
	// possibly with a static utility class
	void createCallbacks();
	static void handleKeys(GLFWwindow* window, int key, int code, int action, int mode);
	static void handleMouse(GLFWwindow* window, double xPos, double yPos);
};

