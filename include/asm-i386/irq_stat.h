/*Author:A.R.Karthick for MIR:
  Maintain IRQ statistics*/
#ifdef __KERNEL__
#ifndef _IRQ_STAT_H
#define _IRQ_STAT_H

struct irq_stat { 
  int count; /*count of irqs*/
};
struct global_irq_stats {
  int effective_count; /*keeps track of whether irq handler is running*/
  int errors;
};
struct global_tasklet_stats { 
  int effective_count; /*serialises tasklets execution*/
};
extern struct irq_stat irq_stat[];
extern struct global_irq_stats global_irq_stats;
extern struct global_tasklet_stats global_tasklet_stats;

#define IRQ_ERR_INC  (++global_irq_stats.errors )
#define IRQ_ERR_COUNT  ( global_irq_stats.errors )
#define IRQ_INC(irq) ( ++irq_stat[irq].count )
#define IRQ_COUNT(irq) ( irq_stat[irq].count )

#define IRQ_ENTER  ( ++global_irq_stats.effective_count )

#define IRQ_LEAVE  ( --global_irq_stats.effective_count )

#define IRQ_EFFECTIVE_COUNT ( global_irq_stats.effective_count)

#define TASKLET_DISABLE ( ++global_tasklet_stats.effective_count )
#define TASKLET_ENABLE  ( --global_tasklet_stats.effective_count )
#define TASKLET_EFFECTIVE_COUNT  ( global_tasklet_stats.effective_count)

#define in_interrupt()  ( IRQ_EFFECTIVE_COUNT > 0  || TASKLET_EFFECTIVE_COUNT > 0 )

#endif
#endif
