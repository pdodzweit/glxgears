#version 330

in vec2 in_position;
in vec4 in_color;
out vec2 position;
out vec4 color;


void main() {
    //We don't do any transforms but here we could apply a transformation to the
    //points such as to move or rotate an object in the scene and do the camera
    //viewpoint. Our input triangle is 2d, but we could have specified 3d  
    //input coordinates.
    //
    //This is actually used by opengl to determine the fragments to render
    //Note that it is a vec4, we have no z, and w is a divider, which is
    //used to scale the rest of the vector. This is a useful output for
    //model transforms. 1.0 places the verticies on the screen edges but
    //we want a more compelling smaller triangle centered in the screen,
    //so we go with 1.5 to reduce the size
    //
    gl_Position = vec4(in_position.x, in_position.y, 0.0, 1.5);
    //We could also change the colors here, but we don't bother
    color = in_color;
}
