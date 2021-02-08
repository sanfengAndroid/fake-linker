package eu.chainfire.libsuperuser;

/**
 * Exception class used to notify developer that a shell was not close()d
 */
@SuppressWarnings("serial")
public class ShellNotClosedException extends RuntimeException {
    public static final String EXCEPTION_NOT_CLOSED = "Application did not close() interactive shell";

    public ShellNotClosedException() {
        super(EXCEPTION_NOT_CLOSED);
    }
}
