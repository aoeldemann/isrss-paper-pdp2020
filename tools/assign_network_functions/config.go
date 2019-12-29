package main

type Config struct {
	Traces  []ConfigTrace  `json:"traces"`
	Actions []ConfigAction `json:"actions"`
	NRuns   int            `json:"n_runs"`
	MaxErr  float64        `json:"max_err"`
}

type ConfigTrace struct {
	FnamePCAP string `json:"pcap"`

	FnameDimensioning string `json:"dimensioning_out"`
	FnameIPP          string `json:"ipp_out"`
	FnameInfo         string `json:"info_out"`
}

type ConfigAction struct {
	IPPBase    int     `json:"ipp_base"`
	IPPPayload int     `json:"ipp_payload"`
	Share      float64 `json:"share"`
}
