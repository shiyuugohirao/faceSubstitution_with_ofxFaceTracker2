#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    ofSetFrameRate(0);
    ofBackground(0);

    vector<ofVideoDevice> devices = grabber.listDevices();
    for(int i = 0; i < devices.size(); i++){
        if(devices[i].bAvailable){
            ofLogNotice() << devices[i].id << ": " << devices[i].deviceName;
        }else{
            ofLogNotice() << devices[i].id << ": " << devices[i].deviceName << " - unavailable ";
        }
    }
    int camW = 640;
    int camH = 480;
    grabber.setDeviceID(1);
    grabber.setup(camW,camH);
    initFbo(partsFbo,camW,camH);
    initFbo(faceFbo,camW,camH);
    initFbo(srcFbo,camW,camH);
    initFbo(maskFbo,camW,camH);

    camTracker.setup();
    srcTracker.setup();

    string srcPath = "obama.jpg";
    src.load(srcPath);
    if(src.isAllocated()){
        srcTracker.update(ofxCv::toCv(src));
    }

    string shaderProgram =
    OS_STRINGIFY(
                 uniform sampler2DRect tex0;
                 uniform sampler2DRect maskTex;
                 void main (void){
                     vec2 pos = gl_TexCoord[0].st;
                     vec3 src = texture2DRect(tex0, pos).rgb;
                     float mask = texture2DRect(maskTex, pos).r;
                     gl_FragColor = vec4( src , mask);
                 });
    maskShader.setupShaderFromSource(GL_FRAGMENT_SHADER, shaderProgram);
    maskShader.linkProgram();

    alpha = 110;

    if(!setFace(srcPointsCoord)){
        ofSystemAlertDialog("failed to find face from \n" + srcPath);
        std::exit(0);
    }
}

//--------------------------------------------------------------
void ofApp::update(){
    grabber.update();
    if(grabber.isFrameNew()){
        camTracker.update(grabber);
    }

    //--- Camera ---//
    if(camTracker.size()){
        auto camFace = camTracker.getInstances()[0];

        ofVboMesh camMesh = remakeFaceMesh(camFace);

        faceFbo.begin();
        ofClear(0);
        src.bind();
        camMesh.draw();
        src.unbind();
        faceFbo.end();

        //--- partsMask ---//
        ofPushStyle();
        ofSetColor(255);
        ofxFaceTracker2Landmarks landmarks = camFace.getLandmarks();
        ofPolyline eyeL = landmarks.getImageFeature(ofxFaceTracker2Landmarks::LEFT_EYE);
        ofPolyline eyeR = landmarks.getImageFeature(ofxFaceTracker2Landmarks::RIGHT_EYE);
        ofPolyline mouth;
        if(alpha < 100){
            mouth = landmarks.getImageFeature(ofxFaceTracker2Landmarks::OUTER_MOUTH);
        }else{
            mouth = landmarks.getImageFeature(ofxFaceTracker2Landmarks::INNER_MOUTH);
        }
        mouth = mouth.getResampledBySpacing(1);
        mouth = mouth.getSmoothed(4);

        ofMesh mesh;
        ofTessellator tess;
        maskFbo.begin();
        {
            ofClear(0);
            tess.tessellateToMesh(mouth, ofPolyWindingMode::OF_POLY_WINDING_ODD, mesh, true);
            mesh.draw();
            tess.tessellateToMesh(eyeL, ofPolyWindingMode::OF_POLY_WINDING_ODD, mesh, true);
            mesh.draw();
            tess.tessellateToMesh(eyeR, ofPolyWindingMode::OF_POLY_WINDING_ODD, mesh, true);
            mesh.draw();
        }
        maskFbo.end();
        ofPopStyle();

        partsFbo.begin();
        ofClear(0);
        maskShader.begin();
        maskShader.setUniformTexture("maskTex", maskFbo.getTexture(), 1 );
        grabber.setAnchorPercent(0, 0);
        grabber.draw(0,0);
        maskShader.end();
        partsFbo.end();
    }

}

//--------------------------------------------------------------
void ofApp::draw(){
#ifndef __OPTIMIZE__
    ofSetColor(ofColor::red);
    ofDrawBitmapString("Warning! Run this app in release mode to get proper performance!",10,60);
    ofSetColor(ofColor::white);
#endif

    alpha = ofMap(mouseX, 10, ofGetWidth()-10, 0, 255, true);
    ofPushMatrix();
    ofTranslate(ofGetCenter());
    ofScale(1.5, 1.5);
    ofSetColor(255);
    if(!ofGetKeyPressed(' ')) {
        grabber.setAnchorPercent(0.5, 0.5);
        grabber.draw(0,0);
    }
    if(camTracker.size()){
        ofSetColor(255, alpha);
        faceFbo.draw(0,0);
        ofSetColor(255);
        partsFbo.draw(0,0);
    }
    ofPopMatrix();

    ofDrawBitmapStringHighlight("fps: "+ofToString(ofGetFrameRate(),2),10,10);
    ofDrawBitmapStringHighlight("Tracker thread framerate : "+ofToString(camTracker.getThreadFps()), 10, 30);
    ofDrawBitmapStringHighlight("alpha: "+ofToString(alpha),10,50);
}

//--------------------------------------------------------------
bool ofApp::setFace(vector<glm::vec2> &pointsCoord){
    int t = ofGetElapsedTimeMicros();
    while(!srcTracker.size()){
        static float t = ofGetElapsedTimef();
        if(src.isAllocated()) srcTracker.update(ofxCv::toCv(src));
        if(ofGetElapsedTimef() -t > 5.0){
            ofLogError("Timeout", "couldn't find Face!");
            return 0;
        }
    }
    ofxFaceTracker2Instance face = srcTracker.getInstances()[0];
    pointsCoord = face.getLandmarks().getImagePoints();

    ofLogNotice("setFace") << ((ofGetElapsedTimeMicros() - t)*0.000001) <<"s";
    return 1;
}

//--------------------------------------------------------------
ofVboMesh ofApp::remakeFaceMesh(ofxFaceTracker2Instance &face){

    vector<glm::vec2> points;
    for(auto &v:face.getLandmarks().getImagePoints()) points.push_back(v);

    //[*] reffered from https://github.com/HalfdanJ/ofxFaceTracker2/issues/25
    unordered_map<string, int> index_map;
    int index = 0;
    for(const auto &p : points){
        string key = to_string(p.x) +","+ to_string(p.y);
        index_map[key] = index;
        index++;
    }

    //--- reset Good TexCoord to face
    ofVboMesh facemesh = face.getLandmarks().getImageMesh();
    facemesh.clearTexCoords();
    for(const auto &v : facemesh.getVertices()){
        string key = to_string(v.x) +","+ to_string(v.y);
        facemesh.addTexCoord(srcPointsCoord[index_map[key]]);
    }

    return facemesh;
}
