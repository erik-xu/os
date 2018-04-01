package hw4;

/**
 * Class to store Process info
 * @author Nicholas Gadjali
 */
public class Process implements Comparable<Process> {
	/**
	 * Process Constructor
	 * @param pName Process name
	 * @param pSize Process Size (MB)
	 * @param arrival Process Arrival Time (ms)
	 * @param duration Service Duration (ms)
	 */
	public Process(String pName, int pSize, int arrival, int duration) {
		this.pName = pName;
		this.pSize = pSize;
		this.arrival = arrival;
		this.duration = duration;
		this.cDuration = duration;
	}
	
	/**
	 * Gets the name of the process
	 * @return process name
	 */
	public String getName() {
		return pName;
	}
	
	/**
	 * Gets the size of the process
	 * @return Process size
	 */
	public int getSize() {
		return pSize;
	}
	
	/**
	 * Gets the arrival time
	 * @return Arrival time (ms)
	 */
	public int getArrTime() {
		return arrival;
	}
	
	/**
	 * Gets the original service duration
	 * @return Original service duration (ms)
	 */
	public int getServiceDur() {
		return duration;
	}
	
	/**
	 * Gets the current service duration
	 * @return Current service duration (ms)
	 */
	public int getCurrentDur() {
		return cDuration;
	}
	
	/**
	 * Simple toString
	 * @return tostring
	 */
	@Override
	public String toString() {
		return " |" + pName + "id," + pSize + "MB," + arrival + "a," + cDuration + "ms| ";
	}
	
	/**
	 * Used to prevent identical arrival times
	 * @param that The process to compare to
	 * @return -1, 0, 1
	 */
	@Override
	public int compareTo(Process that) {
		return this.arrival - that.arrival;
	}
	
	private String pName;
	private int pSize;
	private int arrival;
	private final int duration;
	private int cDuration;
}
