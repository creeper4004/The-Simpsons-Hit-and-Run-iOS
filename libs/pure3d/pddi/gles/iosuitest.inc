// Opt-in GPU regression scene. Runs through the same GL-to-Metal bridge as UI.
// The host checks pixels, rather than treating successful submission as a pass.
static void DrawIosUiRegression(int width, int height)
{
    static GLuint program = 0, vao = 0, vbo = 0;
    static GLint colorLocation = -1, depthLocation = -1;
    if (!program) {
        const char* sources[] = {
            "#version 450 core\nlayout(location=0) in vec2 position; layout(location=0) uniform float z; void main(){gl_Position=vec4(position,z,1.0);}",
            "#version 450 core\nlayout(location=0) out vec4 outputColor; layout(location=1) uniform vec4 tint; void main(){outputColor=tint;}"
        };
        GLuint shaders[2];
        program = glCreateProgram();
        for (int i=0; i<2; ++i) {
            shaders[i]=glCreateShader(i ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
            glShaderSource(shaders[i],1,&sources[i],NULL); glCompileShader(shaders[i]);
            GLint status=0; glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&status);
            if (!status) { fprintf(stderr,"IOS_UI_TEST shader-failure stage=%d\n",i); abort(); }
            glAttachShader(program,shaders[i]);
        }
        glLinkProgram(program);
        GLint status=0; glGetProgramiv(program,GL_LINK_STATUS,&status);
        if (!status) { fprintf(stderr,"IOS_UI_TEST link-failure\n"); abort(); }
        colorLocation=glGetUniformLocation(program,"tint");
        depthLocation=glGetUniformLocation(program,"z");
        if (colorLocation<0 || depthLocation<0) abort();
        glGenVertexArraysOES(1,&vao); glGenBuffers(1,&vbo);
        for(int i=0;i<2;++i) glDeleteShader(shaders[i]);
    }
    glUseProgram(program); glBindVertexArrayOES(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    const float quad[]={-1,-1,1,-1,-1,1,1,1};
    glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STREAM_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),0); glEnableVertexAttribArray(0);
    glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); glDepthMask(GL_TRUE);
    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_ALWAYS);
    glClearColor(0,0,1,1); glClearDepthf(1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const int cells=7;
    auto draw=[&](float r,float g,float b,float a,float z) {
        glUniform4f(colorLocation,r,g,b,a); glUniform1f(depthLocation,z);
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    };
    for(int cell=0;cell<cells;++cell) {
        int left=width*cell/cells, right=width*(cell+1)/cells;
        glViewport(left,0,right-left,height);
        glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
        if(cell==0) draw(0,1,0,1,0);                         // opaque baseline
        if(cell==1) {                                      // transparency over blue
            glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); draw(1,0,0,.5f,0);
        }
        if(cell==2) {                                      // blend state invalidation
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); draw(1,0,0,.5f,0);
            glDisable(GL_BLEND); draw(1,0,0,.25f,0);
        }
        if(cell==3) {                                      // UI must ignore world depth
            glEnable(GL_DEPTH_TEST); glDepthFunc(GL_ALWAYS); draw(1,0,0,1,-.8f);
            glDepthFunc(GL_LESS); glDisable(GL_DEPTH_TEST); draw(0,1,0,1,.8f);
        }
        if(cell==4) {                                      // colorless radar mask across VAO/pass changes
            glEnable(GL_DEPTH_TEST); glDepthFunc(GL_ALWAYS);
            glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
            const float mask[]={-1,-1,0,-1,-1,0, 1,1,0,1,1,0, -1,1,-1,0,0,1, 1,-1,1,0,0,-1};
            glBufferData(GL_ARRAY_BUFFER,sizeof(mask),mask,GL_STREAM_DRAW);
            glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),0);
            glUniform1f(depthLocation,-.8f); glDrawArrays(GL_TRIANGLES,0,12);
            glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STREAM_DRAW);
            glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),0);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            glDepthFunc(GL_LESS); draw(0,1,0,1,0);
        }
        if(cell==5) {                                      // depth clear while depth test is disabled
            glEnable(GL_DEPTH_TEST); glDepthFunc(GL_ALWAYS); draw(1,0,0,1,-.8f);
            glDisable(GL_DEPTH_TEST); glClearDepthf(1); glClear(GL_DEPTH_BUFFER_BIT);
            // A UI draw consumes the clear before the next 3D draw.
            draw(0,0,1,1,0);
            glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); draw(0,1,0,1,0);
        }
        if(cell==6) {                                      // scissor on -> off and restored UI
            glEnable(GL_SCISSOR_TEST); glScissor(left,0,(right-left)/2,height);
            draw(1,0,0,1,0); glDisable(GL_SCISSOR_TEST); draw(1,1,0,1,0);
        }
    }
    glViewport(0,0,width,height); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    glBindVertexArrayOES(0); glUseProgram(0);
    static bool reported=false;
    if(!reported) { fprintf(stderr,"IOS_UI_TEST scene-ready cells=7 size=%dx%d\n",width,height); reported=true; }
}
