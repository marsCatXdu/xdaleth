/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file TransactionBase.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include <libdevcore/vector_ref.h>
#include <libdevcore/Log.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Exceptions.h>
#include "TransactionBase.h"
#include "EVMSchedule.h"

using namespace std;
using namespace dev;
using namespace dev::eth;

// XdAleth marsCatXdu web3的 signTransaction 调用了这个
// 该构造器改造中，正在改造 sign(_s)
TransactionBase::TransactionBase(TransactionSkeleton const& _ts, Secret const& _s):
	m_type(_ts.creation ? ContractCreation : MessageCall),
	m_nonce(_ts.nonce),
	m_value(_ts.value),
	m_receiveAddress(_ts.to),
	m_gasPrice(_ts.gasPrice),
	m_gas(_ts.gas),
	m_data(_ts.data),
	m_extraMsg(_ts.extraMsg);
	m_sender(_ts.from)
{
	if (_s)
		sign(_s);
}

// XdAleth marsCatXdu 从 RLP 构造交易，接收广播的交易应该也是走的这里
TransactionBase::TransactionBase(bytesConstRef _rlpData, CheckTransaction _checkSig)
{
	RLP const rlp(_rlpData);
	try
	{
		if (!rlp.isList())
			BOOST_THROW_EXCEPTION(InvalidTransactionFormat() << errinfo_comment("transaction RLP must be a list"));

		m_nonce = rlp[0].toInt<u256>();
		m_gasPrice = rlp[1].toInt<u256>();
		m_gas = rlp[2].toInt<u256>();
		m_type = rlp[3].isEmpty() ? ContractCreation : MessageCall;
		m_receiveAddress = rlp[3].isEmpty() ? Address() : rlp[3].toHash<Address>(RLP::VeryStrict);
		m_value = rlp[4].toInt<u256>();

		if (!rlp[5].isData())
			BOOST_THROW_EXCEPTION(InvalidTransactionFormat() << errinfo_comment("transaction data RLP must be an array"));

		m_data = rlp[5].toBytes();

		int const v = rlp[6].toInt<int>();
		h256 const r = rlp[7].toInt<u256>();
		h256 const s = rlp[8].toInt<u256>();

		m_extraMsg = rlp[9].toString();		// XdAleth marsCatXdu 还行，RLP自带这个 toString()，吓死我了
											// 我感觉把 v、r、s 串到后面比较好。。但是要是别的东西也用这玩意就炸了，怂，还是续到后面吧

		if (isZeroSignature(r, s))
		{
			m_chainId = v;
			m_vrs = SignatureStruct{r, s, 0};
		}
		else
		{
			if (v > 36)
				m_chainId = (v - 35) / 2; 
			else if (v == 27 || v == 28)
				m_chainId = -4;
			else
				BOOST_THROW_EXCEPTION(InvalidSignature());

			m_vrs = SignatureStruct{r, s, static_cast<byte>(v - (m_chainId * 2 + 35))};

			if (_checkSig >= CheckTransaction::Cheap && !m_vrs->isValid())
				BOOST_THROW_EXCEPTION(InvalidSignature());
		}

		if (_checkSig == CheckTransaction::Everything)
			m_sender = sender();

		if (rlp.itemCount() > 10)	// XdAleth marsCatXdu 整数从9改为10.直接在这里把数一加就扩展了吧？希望不会炸（然而事情好像并没有这么简单）
			BOOST_THROW_EXCEPTION(InvalidTransactionFormat() << errinfo_comment("too many fields in the transaction RLP"));
	}
	catch (Exception& _e)
	{
		_e << errinfo_name("invalid transaction format: " + toString(rlp) + " RLP: " + toHex(rlp.data()));
		throw;
	}
}

Address const& TransactionBase::safeSender() const noexcept
{
	try
	{
		return sender();
	}
	catch (...)
	{
		return ZeroAddress;
	}
}

Address const& TransactionBase::sender() const
{
	if (!m_sender)
	{
		if (hasZeroSignature())
			m_sender = MaxAddress;
		else
		{
			if (!m_vrs)
				BOOST_THROW_EXCEPTION(TransactionIsUnsigned());

			auto p = recover(*m_vrs, sha3(WithoutSignature));
			if (!p)
				BOOST_THROW_EXCEPTION(InvalidSignature());
			m_sender = right160(dev::sha3(bytesConstRef(p.data(), sizeof(p))));
		}
	}
	return m_sender;
}

SignatureStruct const& TransactionBase::signature() const
{ 
	if (!m_vrs)
		BOOST_THROW_EXCEPTION(TransactionIsUnsigned());

	return *m_vrs;
}

// XdAleth marsCatXdu 这玩意感觉不像干了 rlp 的活。。。
void TransactionBase::sign(Secret const& _priv)
{
	auto sig = dev::sign(_priv, sha3(WithoutSignature));		// dev::sign 来自 devcrypto 中的 common
	SignatureStruct sigStruct = *(SignatureStruct const*)&sig;
	if (sigStruct.isValid())
		m_vrs = sigStruct;
}

// XdAleth marsCatXdu 然而这个看起来就非常像是干了 RLP 的活了，对照一下顺序。。
void TransactionBase::streamRLP(RLPStream& _s, IncludeSignature _sig, bool _forEip155hash) const
{
	if (m_type == NullTransaction)
		return;

	_s.appendList((_sig || _forEip155hash ? 3 : 0) + 7);	// 这个是不是限制 RLP 的尺寸的玩意啊，有签名就9个元素，没有就6个。。不对好像没那么简单不管了真麻烦去你的吧
															// 把上面那行结尾的 6 改成 7 了
	_s << m_nonce << m_gasPrice << m_gas;	// 这玩意排序是。。。 nonce, gasPrice, gas, value, data, ???, ???, ???
	if (m_type == MessageCall)				// 最后面三个字段，r v s 看起来非常像算密码用的东西，怕不是验签名用的哟
		_s << m_receiveAddress;				
	else
		_s << "";							// 填数据的时候按交易种类不同做了区分，接收时候没有区分接收。所以这里就直接填个空了
	_s << m_value << m_data;				// 是我的水平太差了吧，看不出来大佬们的一些操作的意义是啥。。。菜哭了。。。。。
											// 到这里前 6 个元素已经填完了
	if (_sig)
	{
		if (!m_vrs)
			BOOST_THROW_EXCEPTION(TransactionIsUnsigned());		// 哦，所以就是说只有签名了的交易才会序列化吧，没签名的话直接炸

		if (hasZeroSignature())									// 这个 hasZeroSignature 和上面的有啥差别。。。不过好像不弄明白也没影响啊
			_s << m_chainId;
		else
		{
			int const vOffset = m_chainId * 2 + 35;
			_s << (m_vrs->v + vOffset);
		}
		_s << (u256)m_vrs->r << (u256)m_vrs->s;					// 第九个元素填完了
	}
	else if (_forEip155hash)		// TODO：查一下这个EIP155，怕是有什么深刻的东西在里面
		_s << m_chainId << 0 << 0;
	
	_s<<m_extraMsg;					// 耶！
}

static const u256 c_secp256k1n("115792089237316195423570985008687907852837564279074904382605163141518161494337");

void TransactionBase::checkLowS() const
{
	if (!m_vrs)
		BOOST_THROW_EXCEPTION(TransactionIsUnsigned());

	if (m_vrs->s > c_secp256k1n / 2)
		BOOST_THROW_EXCEPTION(InvalidSignature());
}

void TransactionBase::checkChainId(int chainId) const
{
	if (m_chainId != chainId && m_chainId != -4)
		BOOST_THROW_EXCEPTION(InvalidSignature());
}

int64_t TransactionBase::baseGasRequired(bool _contractCreation, bytesConstRef _data, EVMSchedule const& _es)
{
	int64_t g = _contractCreation ? _es.txCreateGas : _es.txGas;

	// Calculate the cost of input data.
	// No risk of overflow by using int64 until txDataNonZeroGas is quite small
	// (the value not in billions).
	for (auto i: _data)
		g += i ? _es.txDataNonZeroGas : _es.txDataZeroGas;
	return g;
}

h256 TransactionBase::sha3(IncludeSignature _sig) const
{
	if (_sig == WithSignature && m_hashWith)
		return m_hashWith;

	RLPStream s;
	streamRLP(s, _sig, m_chainId > 0 && _sig == WithoutSignature);

	auto ret = dev::sha3(s.out());
	if (_sig == WithSignature)
		m_hashWith = ret;
	return ret;
}
