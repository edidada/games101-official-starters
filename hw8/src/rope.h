#ifndef ROPE_H
#define ROPE_H
#include "CGL/vector2D.h"
#include "mass.h"
#include "spring.h"
#include <vector>
namespace CGL {
class Rope {
public:
    enum class EulerMethod { Explicit, SemiImplicit };
    // Existing pointer-vector interface borrows the objects.
    Rope(std::vector<Mass*>& points,std::vector<Spring*>& links)
        : masses(points),springs(links),owns(false) {}
    Rope(Vector2D start,Vector2D end,int num_nodes,float node_mass,float k,
         std::vector<int> pinned_nodes);
    ~Rope();
    Rope(const Rope&)=delete;
    Rope& operator=(const Rope&)=delete;
    void simulateEuler(float dt,Vector2D gravity,
                       EulerMethod method=EulerMethod::SemiImplicit);
    void simulateVerlet(float dt,Vector2D gravity);
    void simulateConstraints(float dt,Vector2D gravity,int iterations=32);
    double euler_damping=0.01;
    double verlet_damping=0.00005;
    std::vector<Mass*> masses;
    std::vector<Spring*> springs;
private:
    bool owns=true;
    void springForces();
    void validate(float dt,Vector2D gravity) const;
};
}
#endif
