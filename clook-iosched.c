/*
 * modified by Lukas Borges and Tiago Moore
 *
 * We keep the current disk head sector in clook_dispatch
 * and keep a requests sorted in the add_request method
 * from there everything gets dispatched in a sorted order
 * according to the C-LOOK algorithm
 *
 */


#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

int diskhead = -1;

struct clook_data {
	struct list_head queue;
};

static void clook_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

static int clook_dispatch(struct request_queue *q, int force)
{
	struct clook_data *nd = q->elevator->elevator_data;

	if (!list_empty(&nd->queue)) {
		struct request *rq;
		rq = list_entry(nd->queue.next, struct request, queuelist);
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		diskhead = blk_rq_pos(rq); //assign position to disk head

		//print whether data is being read or write
		char direction;
		if(rq_data_dir(rq) == READ)
			direction = 'R';
		else
			direction = 'W';
		printk("[CLOOK] dsp %c %lu\n", direction, blk_rq_pos(rq));

		return 1;
	}
	return 0;
}

static void clook_add_request(struct request_queue *q, struct request *rq)
{
	struct clook_data *nd = q->elevator->elevator_data;
	struct list_head *cur = NULL;

	list_for_each(cur, &nd->queue) //we advance cur every time
	{
		struct request *c = list_entry(cur, struct request, queuelist);
		if(blk_rq_pos(rq) < diskhead) //request is bigger than disk head
		//we keep servicing bigger requests until we see a small request again
		//we insert when current is smaller than the head and bigger than the request
		{
			if(blk_rq_pos(c) < diskhead &&
			   blk_rq_pos(rq) < blk_rq_pos(c))
				break;
		}
		else //request is bigger than disk head
		{
		//find sport where current is smaller than the head or current smaller than 
		//request
			if(blk_rq_pos(c) < diskhead ||
			   blk_rq_pos(rq) < blk_rq_pos(c))
				break;
		}
	}

	//print whether data is being read or write
	char direction;
	if(rq_data_dir(rq) == READ)
		direction = 'R';
	else
		direction = 'W';
	printk("[CLOOK] add %c %lu\n", direction, blk_rq_pos(rq));

	list_add_tail(&rq->queuelist, cur);

}

static int clook_queue_empty(struct request_queue *q)
{
	struct clook_data *nd = q->elevator->elevator_data;

	return list_empty(&nd->queue);
}

static struct request *
clook_former_request(struct request_queue *q, struct request *rq)
{
	struct clook_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
clook_latter_request(struct request_queue *q, struct request *rq)
{
	struct clook_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static void *clook_init_queue(struct request_queue *q)
{
	struct clook_data *nd;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd)
		return NULL;
	INIT_LIST_HEAD(&nd->queue);
	return nd;
}

static void clook_exit_queue(struct elevator_queue *e)
{
	struct clook_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_clook = {
	.ops = {
		.elevator_merge_req_fn		= clook_merged_requests,
		.elevator_dispatch_fn		= clook_dispatch,
		.elevator_add_req_fn		= clook_add_request,
		.elevator_queue_empty_fn	= clook_queue_empty,
		.elevator_former_req_fn		= clook_former_request,
		.elevator_latter_req_fn		= clook_latter_request,
		.elevator_init_fn		= clook_init_queue,
		.elevator_exit_fn		= clook_exit_queue,
	},
	.elevator_name = "clook",
	.elevator_owner = THIS_MODULE,
};

static int __init clook_init(void)
{
	elv_register(&elevator_clook);

	return 0;
}

static void __exit clook_exit(void)
{
	elv_unregister(&elevator_clook);
}

module_init(clook_init);
module_exit(clook_exit);


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CLOOK IO scheduler");
