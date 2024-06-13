
package dalvik.system;

import java.io.File;
import java.net.URL;
import java.util.Collection;
import java.util.Enumeration;

public class BaseDexClassLoader extends ClassLoader {
  public BaseDexClassLoader(String dexPath, File optimizedDirectory,
                            String librarySearchPath, ClassLoader parent) {
    throw new RuntimeException("Stub!");
  }

  protected Class<?> findClass(String name) throws ClassNotFoundException {
    throw new RuntimeException("Stub!");
  }

  protected URL findResource(String name) {
    throw new RuntimeException("Stub!");
  }

  protected Enumeration<URL> findResources(String name) {
    throw new RuntimeException("Stub!");
  }

  public String findLibrary(String name) {
    throw new RuntimeException("Stub!");
  }

  protected synchronized Package getPackage(String name) {
    throw new RuntimeException("Stub!");
  }

  public String toString() { throw new RuntimeException("Stub!"); }

  public String getLdLibraryPath() {
    throw new UnsupportedOperationException("Stub");
  }

  public void addNativePath(Collection<String> libPaths) {
    throw new UnsupportedOperationException("Stub");
  }
}
