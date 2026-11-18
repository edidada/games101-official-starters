#include <algorithm>
#include <cassert>
#include "BVH.hpp"

BVHAccel::BVHAccel(std::vector<Object*> p, int maxPrimsInNode,
                   SplitMethod splitMethod)
    : maxPrimsInNode(std::min(255, maxPrimsInNode)), splitMethod(splitMethod),
      primitives(std::move(p))
{
    time_t start, stop;
    time(&start);
    if (primitives.empty())
        return;

    root = recursiveBuild(primitives);

    time(&stop);
    double diff = difftime(stop, start);
    int hrs = (int)diff / 3600;
    int mins = ((int)diff / 60) - (hrs * 60);
    int secs = (int)diff - (hrs * 3600) - (mins * 60);

    printf(
        "\rBVH Generation complete: \nTime Taken: %i hrs, %i mins, %i secs\n\n",
        hrs, mins, secs);
}

BVHBuildNode* BVHAccel::recursiveBuild(std::vector<Object*> objects)
{
    BVHBuildNode* node = new BVHBuildNode();

    // Compute bounds of all primitives in BVH node
    Bounds3 bounds;
    for (int i = 0; i < objects.size(); ++i)
        bounds = Union(bounds, objects[i]->getBounds());
    if (objects.size() == 1) {
        // Create leaf _BVHBuildNode_
        node->bounds = objects[0]->getBounds();
        node->object = objects[0];
        node->area = objects[0]->getArea();
        node->left = nullptr;
        node->right = nullptr;
        return node;
    }
    else if (objects.size() == 2) {
        node->left = recursiveBuild(std::vector{objects[0]});
        node->right = recursiveBuild(std::vector{objects[1]});

        node->bounds = Union(node->left->bounds, node->right->bounds);
        node->area = node->left->area + node->right->area;
        return node;
    }
    else {
        Bounds3 centroidBounds;
        for (int i = 0; i < objects.size(); ++i)
            centroidBounds =
                Union(centroidBounds, objects[i]->getBounds().Centroid());
        int dim = centroidBounds.maxExtent();
        switch (dim) {
        case 0:
            std::sort(objects.begin(), objects.end(), [](auto f1, auto f2) {
                return f1->getBounds().Centroid().x <
                       f2->getBounds().Centroid().x;
            });
            break;
        case 1:
            std::sort(objects.begin(), objects.end(), [](auto f1, auto f2) {
                return f1->getBounds().Centroid().y <
                       f2->getBounds().Centroid().y;
            });
            break;
        case 2:
            std::sort(objects.begin(), objects.end(), [](auto f1, auto f2) {
                return f1->getBounds().Centroid().z <
                       f2->getBounds().Centroid().z;
            });
            break;
        }

        size_t split = objects.size() / 2;
        int chosenAxis = dim;
        // Exact sweep SAH. Ct and parent area are constant at this node;
        // minimize Nleft*Aleft + Nright*Aright with one primitive per leaf.
        if (splitMethod == SplitMethod::SAH) {
            double best = std::numeric_limits<double>::infinity();
            for (int axis = 0; axis < 3; ++axis) {
                std::stable_sort(objects.begin(), objects.end(), [axis](Object* a, Object* b) {
                    return a->getBounds().Centroid()[axis] < b->getBounds().Centroid()[axis];
                });
                std::vector<Bounds3> suffix(objects.size());
                suffix.back() = objects.back()->getBounds();
                for (size_t i = objects.size() - 1; i > 0; --i)
                    suffix[i - 1] = Union(suffix[i], objects[i - 1]->getBounds());
                Bounds3 prefix;
                for (size_t i = 1; i < objects.size(); ++i) {
                    prefix = Union(prefix, objects[i - 1]->getBounds());
                    double cost = i * prefix.SurfaceArea() +
                        (objects.size() - i) * suffix[i].SurfaceArea();
                    if (cost < best || (cost == best &&
                        std::abs(double(i) - objects.size()/2.0) <
                        std::abs(double(split) - objects.size()/2.0))) {
                        best = cost;
                        split = i;
                        chosenAxis = axis;
                    }
                }
            }
            std::stable_sort(objects.begin(), objects.end(), [chosenAxis](Object* a, Object* b) {
                return a->getBounds().Centroid()[chosenAxis] < b->getBounds().Centroid()[chosenAxis];
            });
        }
        node->splitAxis = chosenAxis;
        auto beginning = objects.begin();
        auto middling = objects.begin() + split;
        auto ending = objects.end();

        auto leftshapes = std::vector<Object*>(beginning, middling);
        auto rightshapes = std::vector<Object*>(middling, ending);

        assert(objects.size() == (leftshapes.size() + rightshapes.size()));

        node->left = recursiveBuild(leftshapes);
        node->right = recursiveBuild(rightshapes);

        node->bounds = Union(node->left->bounds, node->right->bounds);
        node->area = node->left->area + node->right->area;
    }

    return node;
}

Intersection BVHAccel::Intersect(const Ray& ray) const
{
    Intersection isect;
    if (!root)
        return isect;
    isect = BVHAccel::getIntersection(root, ray);
    return isect;
}

Intersection BVHAccel::getIntersection(BVHBuildNode* node, const Ray& ray) const
{
    if (!node || !node->bounds.IntersectP(ray, ray.direction_inv, {}))
        return {};
    if (node->object) return node->object->getIntersection(ray);
    Intersection left = getIntersection(node->left, ray);
    Ray clipped = ray;
    if (left.happened) clipped.t_max = std::min(clipped.t_max, left.distance);
    Intersection right = getIntersection(node->right, clipped);
    if (!left.happened) return right;
    if (!right.happened) return left;
    return left.distance <= right.distance ? left : right;

}
namespace {
void destroyTree(BVHBuildNode* node) {
    if (!node) return;
    destroyTree(node->left);
    destroyTree(node->right);
    delete node;
}
}
BVHAccel::~BVHAccel() { destroyTree(root); }
Bounds3 BVHAccel::WorldBound() const { return root ? root->bounds : Bounds3(); }
bool BVHAccel::IntersectP(const Ray& ray) const { return Intersect(ray).happened; }

void BVHAccel::getSample(BVHBuildNode* node, float p, Intersection& pos, float& pdf) {
    if (node->object) {
        node->object->Sample(pos, pdf);
        pdf *= node->area;
    } else if (p < node->left->area) {
        getSample(node->left, p, pos, pdf);
    } else {
        getSample(node->right, p-node->left->area, pos, pdf);
    }
}
void BVHAccel::Sample(Intersection& pos, float& pdf) {
    pos = Intersection(); pdf = 0;
    if (!root || !(root->area > 0)) return;
    getSample(root, get_random_float()*root->area, pos, pdf);
    pdf /= root->area;
}
