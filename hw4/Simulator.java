package hw4;

import java.util.LinkedList;
import java.util.Queue;

public class Simulator {
	public static final int PAGES = 100; //Number of starting pages
	
	/**
	 * Constructor
	 * @param prcs Number of processes
	 * @param rt Runtime (ms)
	 */
	public Simulator(int prcs, int rt) {
		this.runtime = rt;
		
		//Generate Workload/Processes
		WorkGen wg = new WorkGen(prcs, rt);
		this.workload = wg.getWorkload();
		
		//Create Pages
		this.freePages = new LinkedList<>();
		for (int i = 0; i < PAGES; i++) {
			freePages.add(new Object());
		}
	}
	
	/**
	 * Main simulation code
	 */
	public void start() {
		
	}
	
	private Queue<Process> workload;
	private LinkedList<Object> freePages;
	private int runtime;
}
