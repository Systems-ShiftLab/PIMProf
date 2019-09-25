//===- PinInstrument.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include "DataReuse.h"

using namespace PIMProf;

/* ===================================================================== */
/* DataReuse */
/* ===================================================================== */

DataReuse::DataReuse()
{
    _root = new TrieNode();
}

DataReuse::~DataReuse()
{
    DeleteTrie(_root);
}

void DataReuse::initialize(ConfigReader &reader)
{
    ReadConfig(reader);
}

void DataReuse::ReadConfig(ConfigReader &reader)
{
    int size = reader.GetInteger("DataReuse", "BatchCount", -1);
    ASSERTX(size >= 0);
    _batchcount = size;

    size = reader.GetInteger("DataReuse", "BatchSize", -1);
    ASSERTX(size > 0);
    _batchsize = size;
}

void DataReuse::UpdateTrie(TrieNode *root, DataReuseSegment &seg)
{
    // A reuse chain segment of size 1 can be removed
    if (seg.size() <= 1) return;

    // seg.print(std::cout);

    TrieNode *curNode = root;
    std::set<BBLID>::iterator it = seg._set.begin();
    std::set<BBLID>::iterator eit = seg._set.end();
    for (; it != eit; it++) {
        BBLID curID = *it;
        TrieNode *temp = curNode->_children[curID];
        if (temp == NULL) {
            temp = new TrieNode();
            temp->_parent = curNode;
            temp->_curID = curID;
            curNode->_children[curID] = temp;
        }
        curNode = temp;
    }
    TrieNode *temp = curNode->_children[seg._headID];
    if (temp == NULL) {
        temp = new TrieNode();
        temp->_parent = curNode;
        temp->_curID = seg._headID;
        curNode->_children[seg._headID] = temp;
        _leaves.push_back(temp);
    }
    temp->_isLeaf = true;
    temp->_count += seg.getCount();
}

void DataReuse::ExportSegment(DataReuseSegment &seg, TrieNode *leaf)
{
    ASSERTX(leaf->_isLeaf);
    seg.setHead(leaf->_curID);
    seg.setCount(leaf->_count);

    TrieNode *temp = leaf;
    while (temp->_parent != NULL) {
        seg.insert(temp->_curID);
        temp = temp->_parent;
    }
}

void DataReuse::DeleteTrie(TrieNode *root)
{
    if (!root->_isLeaf) {
        std::map<BBLID, TrieNode *>::iterator it = root->_children.begin();
        std::map<BBLID, TrieNode *>::iterator eit = root->_children.end();
        for (; it != eit; it++) {
            DataReuse::DeleteTrie(it->second);
        }
    }
    delete root;
}

void DataReuse::PrintTrie(std::ostream &out, TrieNode *root, int parent, int &count)
{
    if (root->_isLeaf) {
        out << "    V_" << count << " [shape=box, label=\"head = " << root->_curID << "\n cnt = " << root->_count << "\"];" << std::endl;
        out << "    V_" << parent << " -> V_" << count << ";" << std::endl;
        parent = count;
        count++;
    }
    else {
        out << "    V_" << count << " [label=\"" << root->_curID << "\"];" << std::endl;
        out << "    V_" << parent << " -> V_" << count << ";" << std::endl;
        parent = count;
        count++;
        std::map<BBLID, TrieNode *>::iterator it = root->_children.begin();
        std::map<BBLID, TrieNode *>::iterator eit = root->_children.end();
        for (; it != eit; it++) {
            DataReuse::PrintTrie(out, it->second, parent, count);
        }
    }
}

std::ostream &DataReuse::print(std::ostream &out, TrieNode *root) {
    int parent = 0;
    int count = 1;
    out << "digraph trie {" << std::endl;
    std::map<BBLID, TrieNode *>::iterator it = root->_children.begin();
    std::map<BBLID, TrieNode *>::iterator eit = root->_children.end();
    out << "    V_0" << " [label=\"root\"];" << std::endl;
    for (; it != eit; it++) {
        DataReuse::PrintTrie(out, it->second, parent, count);
    }
    out << "}" << std::endl;
    return out;
}

