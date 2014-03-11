    // C function EGLDisplay eglGetDisplay ( EGLNativeDisplayType display_id )
    // TODO Deprecate the eglGetDisplay(int) API method

    public static native EGLDisplay eglGetDisplay(
        int display_id
    );
    // TODO Unhide the eglGetDisplay(long) API method
    /**
     * {@hide}
     */
    public static native EGLDisplay eglGetDisplay(
        long display_id
    );

