package hw4;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.Queue;
import java.util.TreeSet;

/**
 * Class for Workload Generation
 * @author Nicholas Gadjali
 */
public class WorkGen {
	public static final int PSIZE[] = {5, 11, 17, 31}; //Predefined Process Sizes
	public static final int PDUR[] = {1, 2, 3, 4, 5}; //Predefined Service Durations
	
	/**
	 * WorkGen Constructor
	 * @param num The number of random processes to create
	 */
	public WorkGen(int num, int rt) {
		this.runtime = rt / 100; //Used to create a range for arrival time generation (ms / 100) for 100ms
		this.workload = new LinkedList<>(createProcesses(num));
	}
	
	/**
	 * Gets the generated workload
	 * @return The generated workload
	 */
	public Queue<Process> getWorkload() {
		return workload;
	}

	/**
	 * Creates processes
	 * @param num The number of random processes to create
	 * @return Unsorted randomly generated processes
	 */
	private ArrayList<Process> createProcesses(int num) {
		TreeSet<Process> wl = new TreeSet<>(); //Will sort processes by arrival time; using Process' compareTo()
		
		int pSizes = PSIZE.length;
		int pDurs = PDUR.length;
		int pName = 0;
		while (wl.size() < num) {
			int test = wl.size();
			wl.add(new Process(pName + "", PSIZE[(int)(Math.random() * pSizes)], 100 * (int)(Math.random() * runtime) , 1000 * PDUR[(int)(Math.random() * pDurs)]));
			if (test != wl.size()) pName++;
		}
		
		ArrayList<Process> wlArrL = new ArrayList<>(wl);
		return wlArrL;
	}
	
	/* test */
	public static void main(String[] args) {
		WorkGen wg = new WorkGen(10, 60000);
		while (!wg.getWorkload().isEmpty()) {
			System.out.println(wg.getWorkload().remove().toString());
		}
	}
	
	private Queue<Process> workload;
	private final int runtime;
}
