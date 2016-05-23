#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class TriangleImageApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
};

void TriangleImageApp::setup()
{
}

void TriangleImageApp::mouseDown( MouseEvent event )
{
}

void TriangleImageApp::update()
{
}

void TriangleImageApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP( TriangleImageApp, RendererGl )
