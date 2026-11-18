#include <iostream>

#include "application.h"
#include "rope.h"

namespace CGL {

Application::Application(AppConfig config) { this->config = config; }

Application::~Application() { delete ropeEuler;delete ropeVerlet; }

void Application::init() {
  // Enable anti-aliasing and circular points.
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_POLYGON_SMOOTH);
  glEnable(GL_POINT_SMOOTH);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
  glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

  glPointSize(8);
  glLineWidth(4);

  glColor3f(1.0, 1.0, 1.0);
  // Create two ropes 
  ropeEuler = new Rope(Vector2D(0, 200), Vector2D(-400, 200), 16, config.mass,
                       config.ks, {0});
  ropeVerlet = new Rope(Vector2D(0, 200), Vector2D(-400, 200), 16, config.mass,
                        config.ks, {0});
}

void Application::render() {
  //Simulation loops
  if(!paused||step_once) {
  for (int i = 0; i < config.steps_per_frame; i++) {
    ropeEuler->simulateEuler(1.f / config.steps_per_frame, config.gravity,euler_method);
    if(constraints) ropeVerlet->simulateConstraints(1.f / config.steps_per_frame,config.gravity);
    else ropeVerlet->simulateVerlet(1.f / config.steps_per_frame, config.gravity);
  }
  ++simulated_frames;step_once=false;
  if(config.frame_limit>0&&simulated_frames==config.frame_limit) {
    paused=true;std::cout<<"FREEZE frames="<<simulated_frames<<std::endl;
  }
  }
  // Rendering ropes
  Rope *rope;

  for (int i = 0; i < 2; i++) {
    if (i == 0) {
      glColor3f(0.0, 0.0, 1.0);
      rope = ropeEuler;
    } else {
      glColor3f(0.0, 1.0, 0.0);
      rope = ropeVerlet;
    }

    glBegin(GL_POINTS);

    for (auto &m : rope->masses) {
      Vector2D p = m->position;
      glVertex2d(p.x, p.y);
    }

    glEnd();

    glBegin(GL_LINES);

    for (auto &s : rope->springs) {
      Vector2D p1 = s->m1->position;
      Vector2D p2 = s->m2->position;
      glVertex2d(p1.x, p1.y);
      glVertex2d(p2.x, p2.y);
    }

    glEnd();

    glFlush();
  }
}

void Application::resize(size_t w, size_t h) {
  screen_width = w;
  screen_height = h;

  float half_width = (float)screen_width / 2;
  float half_height = (float)screen_height / 2;

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-half_width, half_width, -half_height, half_height, 1, 0);
}

void Application::key_event(char key) {
  switch (key) {
  case ' ': paused=!paused;break;
  case 'N': step_once=true;break;
  case '1': euler_method=Rope::EulerMethod::Explicit;break;
  case '2': euler_method=Rope::EulerMethod::SemiImplicit;break;
  case 'C': constraints=!constraints;break;
  case '-':
    if (config.steps_per_frame > 1) {
      config.steps_per_frame /= 2;
    }
    break;
  case '=':
    config.steps_per_frame = std::min(4096,config.steps_per_frame*2);
    break;
  }
  std::cout<<"KEY "<<key<<" steps="<<config.steps_per_frame<<" euler="<<(euler_method==Rope::EulerMethod::Explicit?"explicit":"semi")<<" green="<<(constraints?"constraint":"verlet")<<std::endl;
}

string Application::name() { return "Rope Simulator"; }

string Application::info() {
  ostringstream steps;
  steps << "Steps: " << config.steps_per_frame << " blue=" << (euler_method==Rope::EulerMethod::Explicit?"explicit":"semi") << " green=" << (constraints?"constraint":"verlet") << " paused=" << paused << " [1/2,C,+/-,Space,N]";

  return steps.str();
}
}
