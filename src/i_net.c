#include <i_mem.h>
#include <i_net.h>

/*
static icode_t on_read_handlers(ichannel_t *channel, void **data)
{
	ichannelhandler_t *channelhandler;
	icode_t ret = ISUCCESS;

	ll_for_each_entry(channelhandler,
					  &channel->channelpipeline.channelhandlers, ll) {
		ret = channelhandler->on_read(channel, channelhandler, data);
		if (ret == IFAIL || ret == ISTOLEN)
			break;
	}

	ll_for_each_entry_reverse(channelhandler,
							  &channel->channelpipeline.channelhandlers, ll) {
		ret = channelhandler->on_read_complete(channel, channelhandler, data);
		if (ret == IFAIL || ret == ISTOLEN)
			break;
	}

	return ret;
}
*/

icode_t init_channelpipeline(ichannelpipeline_t *channelpipeline)
{
	channelpipeline->numofhandlers = 0;
	channelpipeline->channelhandler = NULL;

	return ISUCCESS;
}

icode_t set_channel_data(ichannel_t *channel,
						 void *data, data_destroy_f data_destroy)
{
	if (channel->data)
		channel->data_destroy(&channel->data);

	channel->data = data;
	channel->data_destroy = data_destroy;
}

ichannel_t *create_channel(void *data, data_destroy_f data_destroy)
{
	ichannel_t *channel = icalloc(sizeof(ichannel_t));

	init_channelpipeline(&channel->channelpipeline);
	set_channel_data(channel, data, data_destroy);

	return channel;
}

icode_t append_channelhandler(ichannel_t *channel, channelhandler_f on_read,
							  channelhandler_f on_read_complete,
							  channelhandler_f on_write,
							  channelhandler_f on_write_complete,
							  channelhandler_f on_error,
							  channelhandler_f on_error_complete,
							  void *data, data_destroy_f data_destroy)
{
	ichannelpipeline_t *channelpipeline = &channel->channelpipeline;
	ichannelhandler channelhandler;


	return ISUCCESS;
}

