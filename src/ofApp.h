
#pragma once

#include "ofMain.h"
#include "ofxFaceTracker2.h"

class ofApp : public ofBaseApp{
public:
    void setup();
    void update();
    void draw();

    void initFbo(ofFbo &fbo, float w = ofGetWidth(), float h = ofGetHeight()){
        fbo.allocate(w,h);
        fbo.begin();
        ofClear(255,255);
        fbo.end();
        fbo.setAnchorPercent(0.5,0.5);
    }
    ofPoint ofGetCenter(){
        return ofPoint((float)ofGetWidth()/2,(float)ofGetHeight()/2);
    }


private:
    bool setFace(vector<glm::vec2> &pointsCoord);
    ofVboMesh remakeFaceMesh(ofxFaceTracker2Instance &face);

    ofxFaceTracker2 camTracker,srcTracker;
    ofVideoGrabber grabber;
    ofImage src;
    ofFbo partsFbo,faceFbo,maskFbo,srcFbo;
    ofShader maskShader;
    ofPolyline mouth;

    vector<glm::vec2> srcPointsCoord;
    float alpha;
};

