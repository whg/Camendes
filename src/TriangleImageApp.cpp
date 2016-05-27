#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using TriangleRef = shared_ptr<class Triangle>;

class Triangle : public Path2d {
public:

    static TriangleRef create(vec2 p, float sideLength, bool down=true) {

        auto output = make_shared<Triangle>();
        
        output->moveTo(vec2(0));
        output->lineTo(vec2(sideLength, 0));
        output->lineTo(vec2(sideLength * 0.5, sideLength * 0.5f * sqrt(3.0f)));
        output->close();
        
        mat3 transform = glm::translate(mat3(1.0), p);
        
        if (!down) {
            transform = glm::rotate(transform, 3.14159f / 3.0f);
        }
        
        output->transform(transform);
        return output;
    }
    
    
};
class TriGrid {
public:
    TriGrid() {}
    
    void setup(ivec2 size, size_t numTrianglesAcross) {
        mSideLength = static_cast<float>(size.x) / numTrianglesAcross;
        mNumTris.x = std::floor(size.x / mSideLength);
        mNumTris.y = std::floor(size.y / mSideLength);
        
        
        mTriMesh = TriMesh::create(TriMesh::Format().positions(2));
        
        
        for (size_t y = 0; y < mNumTris.y; y++) {
        
            for (size_t i = 0; i < mNumTris.x; i++) {
                vec2 p(i * mSideLength, y * mSideLength * 0.5f * sqrt(3.0f));
                
                if ((y + 1) % 2 == 1) {
                    p.x+= mSideLength * 0.5;
                }
                
                bool doDown = (y) % 2;
                auto downTri = Triangle::create(p, mSideLength, doDown);
                mTriangles.push_back(downTri);
                
                if ((y % 2 == 0 && i != mNumTris.x - 1) || (y % 2 == 1 && i != 0)) {
                    auto upTri = Triangle::create(p, mSideLength, !doDown);
                    mTriangles.push_back(upTri);
                }

            }
        }
        
        for (auto triangle : mTriangles) {
            mTriMesh->appendPositions(&triangle->getPoints()[0], 3);
        }
    }
    
    
    
    TriMeshRef mTriMesh;
    float mSideLength;
    ivec2 mNumTris;
    
    vector<TriangleRef> mTriangles;
};

class TriangleImageApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
    
    TriMeshRef mTriMesh;
    
    TriGrid mTriGrid;
};




void TriangleImageApp::setup()
{
    mTriGrid.setup(getWindowSize(), 20);
}

void TriangleImageApp::mouseDown( MouseEvent event )
{
}

void TriangleImageApp::update()
{
}

void TriangleImageApp::draw()
{
	gl::clear( Color( 1, 1, 1 ) );
    gl::color(0, 0, 0);
    
    gl::pushMatrices();
    gl::translate(vec2(20, 20));
    gl::enableWireframe();
    gl::draw(*mTriGrid.mTriMesh.get());
    
    gl::popMatrices();
//    gl::draw
    
//    auto tri = makeTriangle(vec2(getMousePos() - getWindowPos()), 50);
//    gl::draw(tri);
}

CINDER_APP( TriangleImageApp, RendererGl )
