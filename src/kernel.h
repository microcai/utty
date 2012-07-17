/*
 * kernel.h
 *
 *
 * contain some const defination from kernel. mainly for ioctl
 */

#ifndef KERNEL_H_
#define KERNEL_H_


#define	KDGKBMODE 0x4B44  /* gets current keyboard mode */
#define	KDGKBLED 0x4B64		/* get LED status*/

#define	KDSKBLED	0x4B65		/* set LED status*/


#define           K_UNICODE       0x03

#endif /* KERNEL_H_ */
