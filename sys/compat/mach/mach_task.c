/*-
 * Copyright (c) 2002-2003, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/exec.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sysproto.h>
#include <sys/uio.h>
#include <sys/ktrace.h>


#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <compat/mach/mach_types.h>
#include <compat/mach/mach_message.h>
#include <compat/mach/mach_clock.h>
#include <compat/mach/mach_errno.h>
#include <compat/mach/mach_exec.h>
#include <compat/mach/mach_port.h>
#include <compat/mach/mach_task.h>
#include <compat/mach/mach_services.h>
#include <compat/mach/mach_proto.h>

#include <sys/mach/task_server.h>

#if 0
#ifdef COMPAT_DARWIN
#include <compat/darwin/darwin_exec.h>
#endif

#define ISSET(t, f)     ((t) & (f))

static void
update_exception_port(struct mach_emuldata *, int exc, struct mach_port *);

int
mach_task_get_special_port(struct mach_trap_args *args)
{
	mach_task_get_special_port_request_t *req = args->smsg;
	mach_task_get_special_port_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct thread *td = args->td;
	struct thread *ttd = args->ttd;
	struct mach_emuldata *med;
	struct mach_right *mr;

	med = (struct mach_emuldata *)ttd->td_proc->p_emuldata;

	switch (req->req_which_port) {
	case MACH_TASK_KERNEL_PORT:
		mr = mach_right_get(med->med_kernel, td, MACH_PORT_TYPE_SEND, 0);
		break;

	case MACH_TASK_HOST_PORT:
		mr = mach_right_get(med->med_host, td, MACH_PORT_TYPE_SEND, 0);
		break;

	case MACH_TASK_BOOTSTRAP_PORT:
		mr = mach_right_get(med->med_bootstrap,
		    td, MACH_PORT_TYPE_SEND, 0);
		break;

	case MACH_TASK_WIRED_LEDGER_PORT:
	case MACH_TASK_PAGED_LEDGER_PORT:
	default:
		uprintf("mach_task_get_special_port(): unimpl. port %d\n",
		    req->req_which_port);
		return (mach_msg_error(args, EINVAL));
		break;
	}

	*msglen = sizeof(*rep);
	mach_set_header(rep, req, *msglen);
	mach_add_port_desc(rep, mr->mr_name);
	mach_set_trailer(rep, *msglen);

	return (0);
}

int
mach_ports_lookup(struct mach_trap_args *args)
{
	mach_ports_lookup_request_t *req = args->smsg;
	mach_ports_lookup_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct thread *td = args->td;
	struct thread *ttd = args->ttd;
	struct mach_emuldata *med;
	struct mach_right *mr;
	mach_port_name_t mnp[7];
	void *uaddr;
	int error;
	int count;

	/*
	 * This is some out of band data sent with the reply. In the
	 * encountered situation, the out of band data has always been null
	 * filled. We have to see more of this in order to fully understand
	 * how this trap works.
	 */
	med = (struct mach_emuldata *)ttd->td_proc->p_emuldata;
	mnp[0] = (mach_port_name_t)MACH_PORT_DEAD;
	mnp[3] = (mach_port_name_t)MACH_PORT_DEAD;
	mnp[5] = (mach_port_name_t)MACH_PORT_DEAD;
	mnp[6] = (mach_port_name_t)MACH_PORT_DEAD;

	mr = mach_right_get(med->med_kernel, td, MACH_PORT_TYPE_SEND, 0);
	mnp[MACH_TASK_KERNEL_PORT] = mr->mr_name;
	mr = mach_right_get(med->med_host, td, MACH_PORT_TYPE_SEND, 0);
	mnp[MACH_TASK_HOST_PORT] = mr->mr_name;
	mr = mach_right_get(med->med_bootstrap, td, MACH_PORT_TYPE_SEND, 0);
	mnp[MACH_TASK_BOOTSTRAP_PORT] = mr->mr_name;

	/*
	 * On Darwin, the data seems always null...
	 */
	uaddr = NULL;
	if ((error = mach_ool_copyout(td, &mnp[0],
	    &uaddr, sizeof(mnp), MACH_OOL_TRACE)) != 0)
		return (mach_msg_error(args, error));

	count = 3; /* XXX Shouldn't this be 7? */

	*msglen = sizeof(*rep);
	mach_set_header(rep, req, *msglen);
	mach_add_ool_ports_desc(rep, uaddr, count);

	rep->rep_init_port_set_count = count;

	mach_set_trailer(rep, *msglen);

	return (0);
}

int
mach_task_set_special_port(struct mach_trap_args *args)
{
	mach_task_set_special_port_request_t *req = args->smsg;
	mach_task_set_special_port_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct thread *td = args->td;
	struct thread *ttd = args->ttd;
	mach_port_t mn;
	struct mach_right *mr;
	struct mach_port *mp;
	struct mach_emuldata *med;

	mn = req->req_special_port.name;

	/* Null port ? */
	if (mn == 0)
		return (mach_msg_error(args, 0));

	/* Does the inserted port exists? */
	if ((mr = mach_right_check(mn, td, MACH_PORT_TYPE_ALL_RIGHTS)) == 0)
		return (mach_msg_error(args, EPERM));

	if (mr->mr_type == MACH_PORT_TYPE_DEAD_NAME)
		return (mach_msg_error(args, EINVAL));

	med = (struct mach_emuldata *)ttd->td_proc->p_emuldata;

	switch (req->req_which_port) {
	case MACH_TASK_KERNEL_PORT:
		mp = med->med_kernel;
		med->med_kernel = mr->mr_port;
		if (mr->mr_port != NULL)
			MACH_PORT_REF(mr->mr_port);
		MACH_PORT_UNREF(mp);
		break;

	case MACH_TASK_HOST_PORT:
		mp = med->med_host;
		med->med_host = mr->mr_port;
		if (mr->mr_port != NULL)
			MACH_PORT_REF(mr->mr_port);
		MACH_PORT_UNREF(mp);
		break;

	case MACH_TASK_BOOTSTRAP_PORT:
		mp = med->med_bootstrap;
		med->med_bootstrap = mr->mr_port;
		if (mr->mr_port != NULL)
			MACH_PORT_REF(mr->mr_port);
		MACH_PORT_UNREF(mp);
#ifdef COMPAT_DARWIN
		/*
		 * mach_init sets the bootstrap port for any new process.
		 */
		{
			struct darwin_emuldata *ded;

			ded = ttd->td_proc->p_emuldata;
			if (ded->ded_fakepid == 1) {
				mach_bootstrap_port = med->med_bootstrap;
#ifdef DEBUG_DARWIN
				printf("*** New bootstrap port %p, "
				    "recv %p [%p]\n",
				    mach_bootstrap_port,
				    mach_bootstrap_port->mp_recv,
				    mach_bootstrap_port->mp_recv->mr_sethead);
#endif /* DEBUG_DARWIN */
			}
		}
#endif /* COMPAT_DARWIN */
		break;

	default:
		uprintf("mach_task_set_special_port: unimplemented port %d\n",
		    req->req_which_port);
	}

	*msglen = sizeof(*rep);
	mach_set_header(rep, req, *msglen);

	rep->rep_retval = 0;

	mach_set_trailer(rep, *msglen);

	return (0);
}

int
mach_task_threads(struct mach_trap_args *args)
{
	mach_task_threads_request_t *req = args->smsg;
	mach_task_threads_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct thread *td = args->td;
	struct thread *ttd = args->ttd;
	struct proc *tp = ttd->td_proc;
	struct thread *ctd;
	struct mach_emuldata *med;
	struct mach_thread_emuldata *mle;
	int error;
	void *uaddr;
	size_t size;
	int i = 0;
	struct mach_right *mr;
	mach_port_name_t *mnp;

	med = tp->p_emuldata;
	size = tp->p_numthreads * sizeof(*mnp);
	mnp = malloc(size, M_TEMP, M_WAITOK);
	uaddr = NULL;

	TAILQ_FOREACH(ctd, &tp->p_threads, td_plist) {
		mle = ctd->td_emuldata;
		mr = mach_right_get(mle->mle_kernel, td, MACH_PORT_TYPE_SEND, 0);
		mnp[i++] = mr->mr_name;
	}

	/* This will free mnp */
	if ((error = mach_ool_copyout(td, mnp, &uaddr,
	    size, MACH_OOL_TRACE|MACH_OOL_FREE)) != 0)
		return (mach_msg_error(args, error));

	*msglen = sizeof(*rep);
	mach_set_header(rep, req, *msglen);
	mach_add_ool_ports_desc(rep, uaddr, tp->p_numthreads);

	rep->rep_count = tp->p_numthreads;

	mach_set_trailer(rep, *msglen);

	return (0);
}

int
mach_task_get_exception_ports(struct mach_trap_args *args)
{
	mach_task_get_exception_ports_request_t *req = args->smsg;
	mach_task_get_exception_ports_reply_t *rep = args->rmsg;
	struct thread *td = args->td;
	struct thread *ttd = args->ttd;
	size_t *msglen = args->rsize;
	struct mach_emuldata *med;
	struct mach_right *mr;
	struct mach_exc_info *mei;
	int i, j, count;

	med = ttd->td_proc->p_emuldata;

	/* It always returns an array of 32 ports even if only 9 can be used */
	count = sizeof(rep->rep_old_handler) / sizeof(rep->rep_old_handler[0]);

	mach_set_header(rep, req, *msglen);

	rep->rep_masks_count = count;

	j = 0;
	for (i = 0; i <= EXC_MAX; i++) {
		if (med->med_exc[i] == NULL)
			continue;

		if (med->med_exc[i]->mp_datatype != MACH_MP_EXC_INFO) {
#ifdef DIAGNOSTIC
			printf("Exception port without mach_exc_info\n");
#endif
			continue;
		}
		mei = med->med_exc[i]->mp_data;

		mr = mach_right_get(med->med_exc[i], td, MACH_PORT_TYPE_SEND, 0);

		mach_add_port_desc(rep, mr->mr_name);

		rep->rep_masks[j] = 1 << i;
		rep->rep_old_behaviors[j] = mei->mei_behavior;
		rep->rep_old_flavors[j] = mei->mei_flavor;

		j++;
	}

	*msglen = sizeof(*rep);
	mach_set_trailer(rep, *msglen);

	return (0);
}

static void
update_exception_port(struct mach_emuldata *med, int exc, struct mach_port *mp)
{
	if (med->med_exc[exc] != NULL)
		MACH_PORT_UNREF(med->med_exc[exc]);
	med->med_exc[exc] = mp;
	MACH_PORT_REF(mp);
}

int
mach_task_set_exception_ports(struct mach_trap_args *args)
{
	mach_task_set_exception_ports_request_t *req = args->smsg;
	mach_task_set_exception_ports_reply_t *rep = args->rmsg;
	struct thread *td = args->td;
	struct thread *ttd = args->ttd;
	size_t *msglen = args->rsize;
	struct mach_emuldata *med;
	mach_port_name_t mn;
	struct mach_right *mr;
	struct mach_port *mp;
	struct mach_exc_info *mei;

	mn = req->req_new_port.name;
	if ((mr = mach_right_check(mn, td, MACH_PORT_TYPE_SEND)) == 0)
		return (mach_msg_error(args, EPERM));

	mp = mr->mr_port;
#ifdef DIAGNOSTIC
	if ((mp->mp_datatype != MACH_MP_EXC_INFO) &&
	    (mp->mp_datatype != MACH_MP_NONE))
		printf("mach_task_set_exception_ports: data exists\n");
#endif
	mei = malloc(sizeof(*mei), M_MACH, M_WAITOK);
	mei->mei_flavor = req->req_flavor;
	mei->mei_behavior = req->req_behavior;

	mp->mp_data = mei;
	mp->mp_flags |= MACH_MP_DATA_ALLOCATED;
	mp->mp_datatype = MACH_MP_EXC_INFO;

	med = ttd->td_proc->p_emuldata;
	if (req->req_mask & EXC_MASK_BAD_ACCESS)
		update_exception_port(med, EXC_BAD_ACCESS, mp);
	if (req->req_mask & EXC_MASK_BAD_INSTRUCTION)
		update_exception_port(med, EXC_BAD_INSTRUCTION, mp);
	if (req->req_mask & EXC_MASK_ARITHMETIC)
		update_exception_port(med, EXC_ARITHMETIC, mp);
	if (req->req_mask & EXC_MASK_EMULATION)
		update_exception_port(med, EXC_EMULATION, mp);
	if (req->req_mask & EXC_MASK_SOFTWARE)
		update_exception_port(med, EXC_SOFTWARE, mp);
	if (req->req_mask & EXC_MASK_BREAKPOINT)
		update_exception_port(med, EXC_BREAKPOINT, mp);
	if (req->req_mask & EXC_MASK_SYSCALL)
		update_exception_port(med, EXC_SYSCALL, mp);
	if (req->req_mask & EXC_MASK_MACH_SYSCALL)
		update_exception_port(med, EXC_MACH_SYSCALL, mp);
	if (req->req_mask & EXC_MASK_RPC_ALERT)
		update_exception_port(med, EXC_RPC_ALERT, mp);

#ifdef DEBUG_MACH
	if (req->req_mask & (EXC_ARITHMETIC |
	    EXC_EMULATION | EXC_MASK_SYSCALL |
	    EXC_MASK_MACH_SYSCALL | EXC_RPC_ALERT))
		printf("mach_set_exception_ports: some exceptions are "
		    "not supported (mask %x)\n", req->req_mask);
#endif

	*msglen = sizeof(*rep);
	mach_set_header(rep, req, *msglen);

	rep->rep_retval = 0;

	mach_set_trailer(rep, *msglen);

	return (0);
}

int
mach_task_info(struct mach_trap_args *args)
{
	mach_task_info_request_t *req = args->smsg;
	mach_task_info_reply_t *rep = args->rmsg;
	struct thread *ttd = args->ttd;
	size_t *msglen = args->rsize;
	int count;
	struct proc *tp = ttd->td_proc;

	switch(req->req_flavor) {
	case MACH_TASK_BASIC_INFO: {
		struct mach_task_basic_info *mtbi;
		struct rusage ru;

		count = sizeof(*mtbi) / sizeof(rep->rep_info[0]);
		if (req->req_count < count)
			return (mach_msg_error(args, ENOBUFS));

		ru = tp->p_stats->p_cru;
		mtbi = (struct mach_task_basic_info *)&rep->rep_info[0];
		PROC_LOCK(tp);
		rufetch(tp, &ru);
		PROC_LOCK(tp);

		mtbi->suspend_count = ru.ru_nvcsw + ru.ru_nivcsw;
		mtbi->virtual_size = ru.ru_ixrss;
		mtbi->resident_size = ru.ru_maxrss;
		mtbi->user_time.seconds = ru.ru_utime.tv_sec;
		mtbi->user_time.microseconds = ru.ru_utime.tv_usec;
		mtbi->system_time.seconds = ru.ru_stime.tv_sec;
		mtbi->system_time.microseconds = ru.ru_stime.tv_usec;
		mtbi->policy = 0;

		*msglen = sizeof(*rep) - sizeof(rep->rep_info) + sizeof(*mtbi);
		break;
	}

	/* XXX this is supposed to be about threads, not processes... */
	case MACH_TASK_THREAD_TIMES_INFO: {
		struct mach_task_thread_times_info *mttti;
		struct rusage ru;

		count = sizeof(*mttti) / sizeof(rep->rep_info[0]);
		if (req->req_count < count)
			return (mach_msg_error(args, ENOBUFS));

		ru = tp->p_stats->p_cru;
		mttti = (struct mach_task_thread_times_info *)&rep->rep_info[0];

		mttti->mttti_user_time.seconds = ru.ru_utime.tv_sec;
		mttti->mttti_user_time.microseconds = ru.ru_utime.tv_usec;
		mttti->mttti_system_time.seconds = ru.ru_stime.tv_sec;
		mttti->mttti_system_time.microseconds = ru.ru_stime.tv_usec;

		*msglen = sizeof(*rep) - sizeof(rep->rep_info) + sizeof(*mttti);
		break;
	}

	/* XXX a few statistics missing here */
	case MACH_TASK_EVENTS_INFO: {
		struct mach_task_events_info *mtei;
		struct rusage ru;

		count = sizeof(*mtei) / sizeof(rep->rep_info[0]);
		if (req->req_count < count)
			return (mach_msg_error(args, ENOBUFS));

		mtei = (struct mach_task_events_info *)&rep->rep_info[0];
		ru = tp->p_stats->p_cru;
		PROC_LOCK(tp);
		rufetch(tp, &ru);
		PROC_UNLOCK(tp);

		mtei->mtei_faults = ru.ru_majflt;
		mtei->mtei_pageins = ru.ru_minflt;
		mtei->mtei_cow_faults = 0; /* XXX */
		mtei->mtei_message_sent = ru.ru_msgsnd;
		mtei->mtei_message_received = ru.ru_msgrcv;
		mtei->mtei_syscalls_mach = 0; /* XXX */
		mtei->mtei_syscalls_unix = 0; /* XXX */
		mtei->mtei_csw = 0; /* XXX */

		*msglen = sizeof(*rep) - sizeof(rep->rep_info) + sizeof(*mtei);
		break;
	}

	default:
		uprintf("mach_task_info: unsupported flavor %d\n",
		    req->req_flavor);
		return (mach_msg_error(args, EINVAL));
	};

	mach_set_header(rep, req, *msglen);

	rep->rep_count = count;

	mach_set_trailer(rep, *msglen);

	return (0);
}

int
mach_task_suspend(struct mach_trap_args *args)
{
	mach_task_suspend_request_t *req = args->smsg;
	mach_task_suspend_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct thread *ttd = args->ttd;
	struct thread *tdp;
	struct mach_emuldata *med;
	struct proc *tp = ttd->td_proc;

	med = tp->p_emuldata;
	med->med_suspend++; /* XXX Mach also has a per thread semaphore */

	TAILQ_FOREACH(tdp, &tp->p_threads, td_plist) {
		switch(tdp->td_state) {
		case TDS_INACTIVE:
		case TDS_INHIBITED:
		case TDS_CAN_RUN:
		case TDS_RUNQ:
		case TDS_RUNNING:
			break;
		default:
			return (mach_msg_error(args, 0));
			break;
		}
	}
	PROC_LOCK(tp);
	kern_psignal(tp, SIGSTOP);
	PROC_UNLOCK(tp);

	*msglen = sizeof(*rep);
	mach_set_header(rep, req, *msglen);

	rep->rep_retval = 0;

	mach_set_trailer(rep, *msglen);

	return (0);
}

int
mach_task_resume(struct mach_trap_args *args)
{
	mach_task_resume_request_t *req = args->smsg;
	mach_task_resume_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct thread *ttd = args->ttd;
	struct mach_emuldata *med;
	struct proc *tp = ttd->td_proc;

	med = tp->p_emuldata;
	med->med_suspend--; /* XXX Mach also has a per thread semaphore */
#if 0
	if (med->med_suspend > 0)
		return (mach_msg_error(args, 0)); /* XXX error code */
#endif

	/* XXX We should also wake up the stopped thread... */
#ifdef DEBUG_MACH
	printf("resuming pid %d\n", tp->p_pid);
#endif
	PROC_LOCK(tp);
	(void)kern_psignal(tp, SIGCONT);
	PROC_UNLOCK(tp);

	*msglen = sizeof(*rep);
	mach_set_header(rep, req, *msglen);

	rep->rep_retval = 0;

	mach_set_trailer(rep, *msglen);

	return (0);
}

int
mach_task_terminate(struct mach_trap_args *args)
{
	mach_task_resume_request_t *req = args->smsg;
	mach_task_resume_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct thread *ttd = args->ttd;
	struct sys_exit_args cup;

	cup.rval = 0;
	sys_sys_exit(ttd, &cup);

	*msglen = sizeof(*rep);
	mach_set_header(rep, req, *msglen);

	rep->rep_retval = native_to_mach_errno[0];

	mach_set_trailer(rep, *msglen);

	return (0);
}

int
sys_mach_task_for_pid(struct thread *td, struct mach_task_for_pid_args *uap)
{
	/* {
		syscallarg(mach_port_t) target_tport;
		syscallarg(int) pid;
		syscallarg(mach_port_t) *t;
	} */
	struct mach_right *mr;
	struct mach_emuldata *med;
	struct proc *t;
	int error;

	/*
	 * target_tport is used because the task may be on
	 * a different host. (target_tport, pid) is unique.
	 * We don't support multiple-host configuration
	 * yet, so this parameter should be useless.
	 * However, we still validate it.
	 */
	if ((mr = mach_right_check(uap->target_tport,
	    td, MACH_PORT_TYPE_ALL_RIGHTS)) == NULL)
		return (EPERM);

	if ((t = pfind(uap->pid)) == NULL) {
		return (ESRCH);
	}

	/* Allowed only if the UID match, if setuid, or if superuser */
	if ((t->p_ucred->cr_ruid != td->td_ucred->cr_ruid ||
		 !priv_check(td, PRIV_CRED_SETUID)) != 0) {
		PROC_UNLOCK(t);
		return (EPERM);
	}
	PROC_UNLOCK(t);

	/* XXX: Unlocked, broken. */
	med = t->p_emuldata;
	mr = mach_right_get(med->med_kernel, td, MACH_PORT_TYPE_SEND, 0);
	if (mr) {
		error = copyout(&mr->mr_name, uap->t,
		    sizeof(mr->mr_name));
	} else {
		error = EINVAL;
	}

	return (error);
}

#endif


#define UNSUPPORTED { return (KERN_NOT_SUPPORTED); }

int
task_get_mach_voucher(
	task_t task,
	mach_voucher_selector_t which,
	ipc_voucher_t *voucher
)
UNSUPPORTED;

int
task_get_state(
	task_t task,
	thread_state_flavor_t flavor,
	thread_state_t old_state,
	mach_msg_type_number_t *old_stateCnt
)
UNSUPPORTED;

int
task_policy_get(
	task_t task,
	task_policy_flavor_t flavor,
	task_policy_t policy_info,
	mach_msg_type_number_t *policy_infoCnt,
	boolean_t *get_default
)
UNSUPPORTED;

int
task_policy_set(
	task_t task,
	task_policy_flavor_t flavor,
	task_policy_t policy_info,
	mach_msg_type_number_t policy_infoCnt
)
UNSUPPORTED;

int
task_purgable_info(
	task_t task,
	task_purgable_info_t *stats
)
UNSUPPORTED;

int
task_resume2(
	task_suspension_token_t suspend_token
)
UNSUPPORTED;

int
task_set_mach_voucher(
	task_t task,
	ipc_voucher_t voucher
	)
UNSUPPORTED;

int
task_set_phys_footprint_limit(
	task_t task,
	int new_limit,
	int *old_limit
)
UNSUPPORTED;

int
task_set_state(
	task_t task,
	thread_state_flavor_t flavor,
	thread_state_t new_state,
	mach_msg_type_number_t new_stateCnt
)
UNSUPPORTED;

int
task_suspend2(
	task_t target_task,
	task_suspension_token_t *suspend_token
)
UNSUPPORTED;

int
task_swap_mach_voucher(
	task_t task,
	ipc_voucher_t new_voucher,
	ipc_voucher_t *old_voucher
)
UNSUPPORTED;

int
task_zone_info(
	task_t target_task,
	mach_zone_name_array_t *names,
	mach_msg_type_number_t *namesCnt,
	task_zone_info_array_t *info,
	mach_msg_type_number_t *infoCnt
)
UNSUPPORTED;

int
thread_create_from_user(
	task_t parent_task,
	thread_act_t *child_act
)
UNSUPPORTED;

int
thread_create_running_from_user(
	task_t parent_task,
	thread_state_flavor_t flavor,
	thread_state_t new_state,
	mach_msg_type_number_t new_stateCnt,
	thread_act_t *child_act
)
UNSUPPORTED;

