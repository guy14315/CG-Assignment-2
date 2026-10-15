#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

#include "Libs/Mesh.h"
#include "Libs/Shader.h"
#include "Libs/Window.h"
#include "ProjectPaths.h"

const GLint WIDTH = 800;
const GLint HEIGHT = 600;

float yaw = -90;
float pitch = 0;

std::vector<Mesh *> meshList;
std::vector<Shader *> shaderList;

const std::filesystem::path shaderDirectory =
    std::filesystem::u8path(OPENGL_STARTER_SHADER_DIR);
const std::filesystem::path textureDirectory =
    std::filesystem::u8path(OPENGL_STARTER_TEXTURE_DIR);
const std::filesystem::path modelDirectory =
    std::filesystem::u8path(OPENGL_STARTER_MODEL_DIR);

void CreateOBJ(const std::filesystem::path &modelPath) {
  Mesh *obj = new Mesh();
  if (!obj->CreateMeshFromOBJ(modelPath.u8string(), true)) {
    std::cerr << "Failed to load model: " << modelPath << std::endl;
    delete obj;
    return;
  }
  meshList.push_back(obj);
}

void CreateShaders() {
  Shader *shader = new Shader();
  shader->CreateFromFiles(shaderDirectory / "shader.vert",
                          shaderDirectory / "shader.frag");
  shaderList.push_back(shader);

  Shader *lightShader = new Shader();
  lightShader->CreateFromFiles(shaderDirectory / "lightShader.vert",
                               shaderDirectory / "lightShader.frag");
  shaderList.push_back(lightShader);
}

GLuint LoadTexture(const std::filesystem::path &texturePath) {
  int width = 0, height = 0, channels = 0;
  unsigned char *data =
      stbi_load(texturePath.u8string().c_str(), &width, &height, &channels, 0);
  if (data == nullptr) {
    std::cerr << "Failed to load texture: " << texturePath << std::endl;
    return 0;
  }

  GLenum format;
  switch (channels) {
  case 1:
    format = GL_RED;
    break;
  case 3:
    format = GL_RGB;
    break;
  case 4:
    format = GL_RGBA;
    break;
  default:
    std::cerr << "Unsupported texture format: " << texturePath << std::endl;
    stbi_image_free(data);
    return 0;
  }

  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
               GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);

  stbi_image_free(data);
  glBindTexture(GL_TEXTURE_2D, 0);
  return texture;
}

void UpdateMouseLook(Window &mainWindow) {
  double mouseX, mouseY;
  glfwGetCursorPos(mainWindow.getWindow(), &mouseX, &mouseY);

  static double lastX = mouseX;
  static double lastY = mouseY;

  const float sensitivity = 0.1f;
  yaw += static_cast<float>(mouseX - lastX) * sensitivity;
  pitch += static_cast<float>(mouseY - lastY) * sensitivity;
  pitch = glm::clamp(pitch, -89.0f, 89.0f);

  lastX = mouseX;
  lastY = mouseY;
}

void Cleanup() {
  for (Mesh *mesh : meshList)
    delete mesh;
  meshList.clear();

  for (Shader *shader : shaderList)
    delete shader;
  shaderList.clear();
}

int main() {
  Window mainWindow(WIDTH, HEIGHT, 3, 3);
  if (mainWindow.initialise() != 0)
    return 1;

  glfwSetInputMode(mainWindow.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  CreateOBJ(modelDirectory / "background.obj"); // meshList[0]
  CreateOBJ(modelDirectory / "homeless.obj");   // meshList[1]
  CreateOBJ(modelDirectory / "cube.obj");       // meshList[2], light marker

  if (meshList.empty()) {
    Cleanup();
    return 1;
  }
  CreateShaders();

  GLuint texture = LoadTexture(textureDirectory / "background.jpg");
  GLuint homelessTexture =
      LoadTexture(textureDirectory / "scavenger_textures" /
                  "scavenger_LP_hobo_AlbedoTransparency.png");

  if (texture == 0 || homelessTexture == 0) {
    Cleanup();
    return 1;
  }

  Shader &shader = *shaderList[0];
  Shader &lightShader = *shaderList[1];

  glm::mat4 projection = glm::perspective(glm::radians(70.0f),
                                          (GLfloat)mainWindow.getBufferWidth() /
                                              mainWindow.getBufferHeight(),
                                          0.1f, 100.0f);

  glm::vec3 cameraPos(1.2f, -0.5f,
                      4.0f); // a bit above and back from the person
  const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

  const glm::vec3 personPos(1.5f, -3.0f, 0.0f); // corner, near the left wall

  glm::vec3 lightPos(-1.2f, 2.0f, -1.0f);   // bare bulb above the person
  glm::vec3 lightColour(1.0f, 0.85f, 0.5f); // dirty yellow

  float lastTime = static_cast<float>(glfwGetTime());

  while (!mainWindow.getShouldClose()) {
    float currentTime = static_cast<float>(glfwGetTime());
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    glfwPollEvents();
    UpdateMouseLook(mainWindow);

    // camera
    glm::vec3 cameraDirection(cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                              sin(glm::radians(pitch)),
                              sin(glm::radians(yaw)) *
                                  cos(glm::radians(pitch)));
    glm::vec3 cameraRight =
        glm::normalize(glm::cross(cameraDirection, worldUp));
    glm::vec3 cameraUp =
        glm::normalize(glm::cross(cameraRight, cameraDirection));

    GLFWwindow *window = mainWindow.getWindow();
    float moveSpeed = 5.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      cameraPos += cameraDirection * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      cameraPos -= cameraDirection * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      cameraPos -= cameraRight * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      cameraPos += cameraRight * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window, GLFW_TRUE);

    glm::mat4 view =
        glm::lookAt(cameraPos, cameraPos + cameraDirection, cameraUp);

    glClearColor(0.03f, 0.04f, 0.06f, 1.0f); // near-black
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::vec3 bulbColour = lightColour;

    shader.UseShader();
    glUniformMatrix4fv(shader.GetUniformLocation("view"), 1, GL_FALSE,
                       glm::value_ptr(view));
    glUniformMatrix4fv(shader.GetUniformLocation("projection"), 1, GL_FALSE,
                       glm::value_ptr(projection));
    glUniform3fv(shader.GetUniformLocation("lightColour"), 1,
                 glm::value_ptr(bulbColour));
    glUniform3fv(shader.GetUniformLocation("lightPos"), 1,
                 glm::value_ptr(lightPos));
    glUniform3fv(shader.GetUniformLocation("viewPos"), 1,
                 glm::value_ptr(cameraPos));
    glUniform1i(shader.GetUniformLocation("texture_data"), 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glm::mat4 model(1.0f);
    glUniformMatrix4fv(shader.GetUniformLocation("model"), 1, GL_FALSE,
                       glm::value_ptr(model));
    meshList[0]->RenderMesh();

    if (meshList.size() > 1) {
      glBindTexture(GL_TEXTURE_2D, homelessTexture);
      model = glm::translate(glm::mat4(1.0f), personPos);
      model = glm::rotate(model, glm::radians(90.0f),
                          glm::vec3(0.0f, -1.0f, 0.0f)); // turn
      model = glm::scale(model, glm::vec3(0.02f));
      glUniformMatrix4fv(shader.GetUniformLocation("model"), 1, GL_FALSE,
                         glm::value_ptr(model));
      meshList[1]->RenderMesh();
    }

    if (meshList.size() > 2) {
      lightShader.UseShader();
      model = glm::translate(glm::mat4(1.0f), lightPos);
      model = glm::scale(model, glm::vec3(0.1f));
      glUniformMatrix4fv(lightShader.GetUniformLocation("model"), 1, GL_FALSE,
                         glm::value_ptr(model));
      glUniformMatrix4fv(lightShader.GetUniformLocation("view"), 1, GL_FALSE,
                         glm::value_ptr(view));
      glUniformMatrix4fv(lightShader.GetUniformLocation("projection"), 1,
                         GL_FALSE, glm::value_ptr(projection));
      glUniform3fv(lightShader.GetUniformLocation("lightColour"), 1,
                   glm::value_ptr(bulbColour));
      meshList[2]->RenderMesh();
    }

    glUseProgram(0);
    mainWindow.swapBuffers();
  }

  glDeleteTextures(1, &texture);
  glDeleteTextures(1, &homelessTexture);
  Cleanup();
  return 0;
}
